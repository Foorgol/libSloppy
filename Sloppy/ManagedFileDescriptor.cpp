/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2017  Volker Knollmann
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

#include "ManagedFileDescriptor.h"

namespace Sloppy
{
  constexpr int ManagedFileDescriptor::DefaultReadWait_ms;

  ManagedFileDescriptor::ManagedFileDescriptor(int _fd)
    :fd{_fd}, fdMutex{}, st{State::Idle}, readBuf{nullptr}
  {
    if (fd < 0)
    {
      throw InvalidDescriptor{};
    }

    readBuf = (char *)malloc(ReadChunkSize);
    if (readBuf == nullptr)
    {
      ::close(fd);
      throw OutOfMemory{};
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

    if (readBuf != nullptr) free(readBuf);
  }

  //----------------------------------------------------------------------------

  bool ManagedFileDescriptor::blockingWrite(const string &data)
  {
    return blockingWrite(data.c_str(), data.size());
  }

  //----------------------------------------------------------------------------

  bool ManagedFileDescriptor::blockingWrite(const ManagedMemory& data)
  {
    return blockingWrite(data.get_c(), data.getSize());
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

    return (((size_t) n) == len);
  }


  //----------------------------------------------------------------------------

  string ManagedFileDescriptor::blockingRead(size_t minLen, size_t maxLen, size_t timeout_ms)
  {
    if (minLen == 0) minLen = 1;   // zero means: no min length which is equivalent to "at least one byte"
    if ((maxLen > 0) && (minLen > maxLen))
    {
      throw InvalidDataSize{};
    }

    // prepare the result string
    string result;

    // special case: if the result length is fixed, use
    // a direct-write-to-buffer version
    if (minLen == maxLen)
    {
      result.resize(maxLen);
      blockingRead((char *)result.c_str(), maxLen, timeout_ms);
      return result;
    }

    // start a stop watch
    auto startTime = chrono::high_resolution_clock::now();

    // wait for the fd to become available
    lock_guard<mutex> lockFd{fdMutex};

    // just to be sure: check the state
    if (st != State::Idle) return string{};

    // do the actual read
    st = State::Reading;
    while (true)
    {
      // determine the maximum number of bytes to be read. do not read
      // more than requested. do not read more than what fits into the buffer
      size_t nMax = (maxLen > 0) ? (maxLen - result.size()) : ReadChunkSize;
      if (nMax > ReadChunkSize) nMax = ReadChunkSize;

      // if there is a timeout specified, calculate the remaining time
      // we may wait for data to become available
      if (timeout_ms > 0)
      {
        auto _elapsedTime = chrono::high_resolution_clock::now() - startTime;
        size_t elapsedTime = chrono::duration_cast<chrono::milliseconds>(_elapsedTime).count();

        if (elapsedTime > timeout_ms)
        {
          st = State::Idle;
          throw ReadTimeout{result};
        }

        // wait for data to become available
        size_t remainingTime = timeout_ms - elapsedTime;
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
          throw ReadTimeout{result};
        }

        // continue with reading the data...
      }

      int n = read(fd, readBuf, nMax);

      if (n > 0)
      {
        result += string{readBuf, (size_t)n};
      }
      if (n == 0)  // descriptor is non-blocking
      {
        this_thread::sleep_for(chrono::milliseconds{DefaultReadWait_ms});
      }
      if (n < 0)
      {
        throw IOError{};
      }

      if (result.size() >= minLen) break;
    }

    st = State::Idle;

    return result;
  }

  //----------------------------------------------------------------------------

  ManagedBuffer ManagedFileDescriptor::blockingRead_MB(size_t expectedLen, size_t timeout_ms)
  {
    if (expectedLen == 0)
    {
      throw InvalidDataSize{};
    }

    // prepare the result buffer
    ManagedBuffer result{expectedLen};

    // do the actual read
    try
    {
      blockingRead(result.get_c(), expectedLen, timeout_ms);
    }
    catch (ReadTimeout ex)
    {
      result.shrink(ex.getNumBytesRead());
      throw ReadTimeout{result.copyToString()};
    }

    return result;
  }

  //----------------------------------------------------------------------------

  size_t ManagedFileDescriptor::blockingRead(char* buf, size_t expectedLen, size_t timeout_ms)
  {
    if (expectedLen == 0)
    {
      throw InvalidDataSize{};
    }

    size_t offset = 0;

    // start a stop watch
    auto startTime = chrono::high_resolution_clock::now();

    // wait for the fd to become available
    lock_guard<mutex> lockFd{fdMutex};
    if (st != State::Idle) return 0;

    // do the actual read
    st = State::Reading;
    while (offset != expectedLen)
    {
      // determine the maximum number of bytes to be read.
      size_t remainingBytes = expectedLen - offset;

      // if there is a timeout specified, calculate the remaining time
      // we may wait for data to become available
      if (timeout_ms > 0)
      {
        auto _elapsedTime = chrono::high_resolution_clock::now() - startTime;
        size_t elapsedTime = chrono::duration_cast<chrono::milliseconds>(_elapsedTime).count();

        if (elapsedTime > timeout_ms)
        {
          st = State::Idle;
          throw ReadTimeout{offset};
        }

        // wait for data to become available
        size_t remainingTime = timeout_ms - elapsedTime;
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
          throw ReadTimeout{offset};
        }

        // continue with reading the data...
      }

      int n = read(fd, buf + offset, remainingBytes);
      offset += n;

      if (n == 0)  // descriptor is non-blocking
      {
        this_thread::sleep_for(chrono::milliseconds{DefaultReadWait_ms});
      }
      if (n < 0)
      {
        throw IOError{};
      }
    }

    st = State::Idle;

    return offset;
  }

  //----------------------------------------------------------------------------

  void ManagedFileDescriptor::close()
  {
    // wait for the fd to become available
    lock_guard<mutex> lockFd{fdMutex};

    int rc = ::close(fd);
    fd = -1;
    if (rc < 0) throw IOError{};
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
    if (readBuf != nullptr)
    {
      free(readBuf);
      readBuf = nullptr;
    }

    return tmp;
  }

  //----------------------------------------------------------------------------

  bool waitForReadOnDescriptor(int fd, size_t timeout_ms)
  {
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    fd_set readFd;
    FD_ZERO(&readFd);
    FD_SET(fd, &readFd);
    int retVal = select(fd+1, &readFd, nullptr, nullptr, &tv);

    // evaluate the result
    if (retVal < 0)
    {
      throw IOError{};
    }

    return (retVal > 0);
  }

}
