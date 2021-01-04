/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2019  Volker Knollmann
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <cstdlib>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <thread>

#include "ManagedFileDescriptor.h"
#include "Timer.h"

using namespace std;

namespace Sloppy
{
  ManagedFileDescriptor::ManagedFileDescriptor(int _fd, size_t readBufferSize)
    :fd{_fd}, fdMutex{}, st{State::Idle}, defaultReadBufSize{readBufferSize}
  {
    if (fd < 0)
    {
      throw InvalidDescriptor{};
    }
  }

  //----------------------------------------------------------------------------

  ManagedFileDescriptor::~ManagedFileDescriptor()
  {
    // wait for the fd to become available
    lock_guard<mutex> lockFd{fdMutex};

    if ((st != State::Closed) && (fd >= 0))
    {
      ::close(fd);
    }
  }

  //----------------------------------------------------------------------------

  bool ManagedFileDescriptor::blockingWrite(const string &data)
  {
    return blockingWrite(data.c_str(), data.size());
  }

  //----------------------------------------------------------------------------

  bool ManagedFileDescriptor::blockingWrite(const MemView& data)
  {
    return blockingWrite(data.to_charPtr(), data.size());
  }

  //----------------------------------------------------------------------------

  bool ManagedFileDescriptor::blockingWrite(const char* ptr, const size_t len)
  {
    // wait for the fd to become available
    lock_guard<mutex> lockFd{fdMutex};

    // just to be sure: check the state
    if (st != State::Idle)
    {
      cerr << "FD lock acquired, but FD not idle!" << endl;
      return false;
    }

    // do the actual write
    st = State::Writing;
    const int n = write(fd, ptr, len);
    st = State::Idle;

    if (n < 0)
    {
      throw IOError{};
    }
    if (static_cast<size_t>(n) != len)
    {
      cerr << "FD write: only " << n << " of " << len << " bytes written!" << endl;
    }

    return (static_cast<size_t>(n) == len);
  }


  //----------------------------------------------------------------------------

  MemArray ManagedFileDescriptor::blockingRead(size_t minLen, const size_t maxLen, const int timeout_ms)
  {
    if (minLen == 0) minLen = 1;   // zero means: no min length which is equivalent to "at least one byte"
    if ((maxLen > 0) && (minLen > maxLen))
    {
      throw InvalidDataSize{};
    }

    // prepare the result buffer
    //
    // apply some "intelligence" that avoids too
    // frequent resize()-operations of the buffer
    // and that avoids allocating too much memory.
    size_t initialResultBufSize = defaultReadBufSize;
    if (minLen == maxLen)
    {
      initialResultBufSize = minLen;
    }
    if ((maxLen > 0) && (maxLen < initialResultBufSize))
    {
      initialResultBufSize = maxLen;
    }
    MemArray result{initialResultBufSize};

    // start a stop watch
    Timer readTimer;
    if (timeout_ms > 0) readTimer.setTimeoutDuration__ms(timeout_ms);

    // wait for the fd to become available
    lock_guard<mutex> lockFd{fdMutex};

    // just to be sure: check the state
    if (st != State::Idle)
    {
      throw std::runtime_error("ManagedFileDescriptor: unexpected, inconsistent FD state!");
    }

    // do the actual read
    st = State::Reading;
    size_t bytesRead = 0;
    while (true)
    {
      // limit the waiting time for the read operation
      // to the timeout value. In case we need multiple read() calls
      // to collect the requested number of bytes, we need to
      // calculate how much time is left.
      int actualTimeout = timeout_ms;
      if (timeout_ms > 0)
      {
        if (readTimer.isElapsed())
        {
          st = State::Idle;
          throw ReadTimeout{result.view()};
        }

        // calcuate the remaining time
        actualTimeout = timeout_ms - static_cast<int>(readTimer.getTime__ms());
        if (actualTimeout < 0) actualTimeout = 0;   // avoid blocking if the time has elapsed in the meantime
      }

      // execute a single read and write the result directly into
      // the result buffer
      bytesRead += readSingleShot(result, bytesRead, actualTimeout);

      if (bytesRead >= minLen) break;
    }

    st = State::Idle;

    result.resize(bytesRead);
    return result;
  }

  //----------------------------------------------------------------------------

  MemArray ManagedFileDescriptor::blockingRead_FixedSize(size_t expectedLen, size_t timeout_ms)
  {
    if (expectedLen == 0)
    {
      throw InvalidDataSize{};
    }

    return blockingRead(expectedLen, expectedLen, timeout_ms);
  }

  //----------------------------------------------------------------------------

  void ManagedFileDescriptor::close()
  {
    // wait for the fd to become available
    lock_guard<mutex> lockFd{fdMutex};

    int rc = ::close(fd);
    fd = -1;
    if (rc < 0) throw IOError{};

    st = State::Closed;
  }

  //----------------------------------------------------------------------------

  ManagedFileDescriptor::State ManagedFileDescriptor::getState()
  {
    return st;
  }

  //----------------------------------------------------------------------------

  int ManagedFileDescriptor::releaseDescriptor()
  {
    // wait for the fd to become available
    lock_guard<mutex> lockFd{fdMutex};

    // just to be sure: check the state
    if (st != State::Idle) return -1;

    // return the descriptor in state "idle" and
    // set the internal descriptor to an invalid
    // value. the object should be used anymore
    // after this operation

    int tmp = fd;
    fd = -1;

    return tmp;
  }

  //----------------------------------------------------------------------------

  bool ManagedFileDescriptor::waitForInput(int timeout_ms)
  {
    PollFlags reqFlags;
    reqFlags.in = true;
    const auto outFlags = poll(reqFlags, timeout_ms);
    if (!outFlags) return false;
    return outFlags->in;
  }

  //----------------------------------------------------------------------------

  std::optional<PollFlags> ManagedFileDescriptor::poll(const PollFlags& reqFlags, int timeout_ms)
  {
    Sloppy::Timer t;

    // wait for the fd to become available
    lock_guard<mutex> lockFd{fdMutex};

    // just to be sure: check the state
    if (st != State::Idle)
    {
      throw std::runtime_error("ManagedFileDescriptor: unexpected, inconsistent FD state!");
    }

    // calculate the remaining time after waiting for the lock
    const int actualTimeout = timeout_ms - static_cast<int>(t.getTime__ms());
    if (actualTimeout < 0) return {}; // timeout occurred already while waiting for the mutex

    return poll_internal_noMutex(reqFlags, actualTimeout);
  }

  //----------------------------------------------------------------------------

  std::optional<PollFlags> ManagedFileDescriptor::poll_internal_noMutex(const PollFlags& reqFlags, int timeout_ms)
  {
    pollfd pfd;
    pfd.fd = fd;
    pfd.events = reqFlags.toInt();

    st = State::Polling;
    const int rc = ::poll(&pfd, 1, timeout_ms);
    st = State::Idle;

    if (rc == -1)
    {
      throw IOError{};
    }

    if (rc == 0) return {};

    return PollFlags{pfd.revents};
  }

  //----------------------------------------------------------------------------

  size_t ManagedFileDescriptor::readSingleShot(MemArray& buf, size_t idxForStorage, int timeout_ms)
  {
    if (idxForStorage >= buf.size())
    {
      throw std::out_of_range("ManagedFileDescriptor::readSingleShot(): inconsistent parameters, storage index exceeds buffer size");
    }

    // wait for data to become available
    PollFlags pf;
    pf.in = true;
    try
    {
      auto outFlags = poll_internal_noMutex(pf, timeout_ms);
      const bool hasData = outFlags ? outFlags->in : false;
      if (!hasData) return 0;
    }
    catch (IOError)
    {
      st = State::Idle;
      throw;
    }

    // execute the actual read
    const auto nMax = buf.size() - idxForStorage;
    auto it = buf.begin();
    std::advance(it, idxForStorage);
    int n = read(fd, it, nMax);

    if (n < 0)
    {
      throw IOError{};
    }

    return n;
  }

  //----------------------------------------------------------------------------

  ManagedFileDescriptor::ManagedFileDescriptor(ManagedFileDescriptor&& other)
  {
    // secure exclusive access to the FDs
    lock_guard<mutex> lockFd_other{other.fdMutex};
    lock_guard<mutex> lockFd_Self{fdMutex};

    // transfer ownership
    fd = other.fd;
    other.fd = -1;
    State tmp = other.st;
    st = tmp;
    other.st = State::Closed;
  }

  //----------------------------------------------------------------------------

  ManagedFileDescriptor& ManagedFileDescriptor::operator=(ManagedFileDescriptor&& other)
  {
    // secure exclusive access to the FDs
    lock_guard<mutex> lockFd_other{other.fdMutex};
    lock_guard<mutex> lockFd_Self{fdMutex};

    // close our existing FD, if any
    if (fd >= 0)
    {
      int rc = ::close(fd);
      fd = -1;
      if (rc < 0) throw IOError{};
    }

    // transfer ownership
    fd = other.fd;
    other.fd = -1;
    State tmp = other.st;
    st = tmp;
    other.st = State::Closed;

    return *this;
  }

  //----------------------------------------------------------------------------

  bool waitForReadOnDescriptor(int fd, size_t timeout_ms)
  {
    fd_set readFd;
    FD_ZERO(&readFd);
    FD_SET(fd, &readFd);

    int retVal;
    if (timeout_ms > 0)
    {
      timeval tv;
      tv.tv_sec = timeout_ms / 1000;
      tv.tv_usec = (timeout_ms % 1000) * 1000;

      retVal = select(fd+1, &readFd, nullptr, nullptr, &tv);
    } else {
      retVal = select(fd+1, &readFd, nullptr, nullptr, nullptr);
    }

    // evaluate the result
    if (retVal < 0)
    {
      throw IOError{};
    }

    return (retVal > 0);
  }

  //----------------------------------------------------------------------------

  PollFlags::PollFlags(int events)
  {
    in = events & POLLIN;
    pri = events & POLLPRI;
    out = events & POLLOUT;
    rdhup = events & POLLRDHUP;
    err = events & POLLERR;
    hup = events & POLLHUP;
    nval = events & POLLNVAL;
  }

  //----------------------------------------------------------------------------

  short PollFlags::toInt() const
  {
    short result{0};

    if (in) result |= POLLIN;
    if (pri) result |= POLLPRI;
    if (out) result |= POLLOUT;
    if (rdhup) result |= POLLRDHUP;
    if (err) result |= POLLERR;
    if (hup) result |= POLLHUP;
    if (nval) result |= POLLNVAL;

    return result;
  }

}
