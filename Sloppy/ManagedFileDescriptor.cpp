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
  constexpr int ManagedFileDescriptor::DefaultReadWait_ms;

  ManagedFileDescriptor::ManagedFileDescriptor(int _fd, size_t readBufferSize)
    :fd{_fd}, fdMutex{}, st{State::Idle}, readBuf{readBufferSize}
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

  bool ManagedFileDescriptor::blockingWrite(const char* ptr, size_t len)
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
    int n = write(fd, ptr, len);
    st = State::Idle;

    if (n < 0)
    {
      throw IOError{};
    }
    if (n != len)
    {
      cerr << "FD write: only " << n << " of " << len << " bytes written!" << endl;
    }

    return (n == len);
  }


  //----------------------------------------------------------------------------

  MemArray ManagedFileDescriptor::blockingRead(size_t minLen, size_t maxLen, size_t timeout_ms)
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
    size_t initialResultBufSize = ReadChunkSize;
    if (minLen == maxLen)
    {
      initialResultBufSize = minLen;
    }
    if (initialResultBufSize > maxLen)
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
      if (timeout_ms > 0)
      {
        if (readTimer.isElapsed())
        {
          st = State::Idle;
          throw ReadTimeout{result.view()};
        }

        // wait for data to become available
        size_t remainingTime = timeout_ms - readTimer.getTime__ms();
        bool hasData;
        try
        {
          hasData = waitForReadOnDescriptor(fd, remainingTime);
        }
        catch (IOError)
        {
          st = State::Idle;
          throw;
        }

        // evaluate the result
        if (!hasData)
        {
          st = State::Idle;
          throw ReadTimeout{result.view()};
        }

        // at this point, the descriptor is ready for reading
        // and the timeout has not yet been reached.
      }

      // determine the maximum number of bytes to be read. do not read
      // more than requested. do not read more than what fits into our read buffer
      size_t nMax = (maxLen > 0) ? (maxLen - bytesRead) : readBuf.size();
      if (nMax > readBuf.size()) nMax = readBuf.size();

      int n = read(fd, readBuf.to_charPtr(), nMax);

      if (n > 0)   /// the read succeeded
      {
        // does the received data fit into the result
        // buffer or is a resize()-operation necessary
        if ((bytesRead + n) > result.size())
        {
          size_t newSize = (bytesRead + n) * 1.2;  // 20 % additional space
          result.resize(newSize);
        }
        result.copyOver(readBuf.view().slice_byCount(0, n), bytesRead);
        bytesRead += n;
      }
      if (n == 0)  // descriptor is non-blocking
      {
        this_thread::sleep_for(chrono::milliseconds{DefaultReadWait_ms});
      }
      if (n < 0)
      {
        throw IOError{};
      }

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

    readBuf.releaseMemory();
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
    readBuf.releaseMemory();

    return tmp;
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
    readBuf = std::move(other.readBuf);  // re-use MemArray's move assignment
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

    // release our read buffer, if any
    //
    // should also be handled by MemArray's move
    // assignment operator, but better be safe than
    // sorry
    if (readBuf.notEmpty())
    {
      readBuf.releaseMemory();
    }

    // transfer ownership
    fd = other.fd;
    other.fd = -1;
    State tmp = other.st;
    st = tmp;
    other.st = State::Closed;
    readBuf = std::move(other.readBuf);  // re-use MemArray's move assignment

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

}
