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

#ifndef __LIBSLOPPY_MANAGEDFD_H
#define __LIBSLOPPY_MANAGEDFD_H

#include <string>
#include <cstring>

#include "libSloppy.h"

using namespace std;

namespace Sloppy
{
  //
  // a few exceptions
  //
  class InvalidDescriptor{};

  class InvalidDataSize{};

  class OutOfMemory{};

  class ReadTimeout
  {
  public:
    explicit ReadTimeout(const string& incompleteData)
      :data{incompleteData}, len{incompleteData.size()}{}
    explicit ReadTimeout(const size_t& incompleteDataLen)
      :data{}, len{incompleteDataLen}{}

    string getIncompleteData() const { return data; }
    size_t getNumBytesRead() const { return data.size(); }

  private:
    string data;
    size_t len;
  };

  class IOError
  {
  public:
    IOError()
      :e{errno}, eStr{strerror(errno)}{}

    IOError(int nError, const string& errStr)
      :e{nError}, eStr{errStr}{}

    int getErrorNumber() const { return e; }
    string getErrString() const { return eStr; }

  private:
    int e;
    string eStr;
  };

  //
  // the actual lib functions
  //

  bool waitForReadOnDescriptor(int fd, size_t timeout_ms);

  class ManagedFileDescriptor
  {
  public:
    static constexpr size_t ReadChunkSize = 512000;
    static constexpr int DefaultReadWait_ms = 10;

    enum class State
    {
      Idle,
      Reading,
      Writing,
      Closed
    };

    ManagedFileDescriptor(int _fd);
    virtual ~ManagedFileDescriptor();

    bool blockingWrite(const string& data);
    bool blockingWrite(const ManagedMemory& data);
    bool blockingWrite(const char* ptr, size_t len);

    string blockingRead(size_t minLen, size_t maxLen = 0, size_t timeout_ms = 0);
    ManagedBuffer blockingRead_MB(size_t expectedLen, size_t timeout_ms = 0);
    size_t blockingRead(char* buf, size_t expectedLen, size_t timeout_ms = 0);

    void close();

    State getState();

    int releaseDescriptor();

  protected:
    int fd;
    mutex fdMutex;
    atomic<State> st;
    char* readBuf;

  };
}

#endif
