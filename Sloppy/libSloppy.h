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

#ifndef __LIBSLOPPY_SLOPPY_H
#define __LIBSLOPPY_SLOPPY_H

#include <string>
#include <vector>
#include <algorithm>
#include <mutex>
#include <atomic>
#include <thread>
#include <cstring>

using namespace std;

namespace Sloppy
{
  using StringList = vector<string>;

  // split strings by delimiter string (easier to handle than Boost's function)
  void stringSplitter(StringList& target, const string& src, const string& delim, bool trimStrings=false);

  // replace substrings (similar to what Boost provides)
  bool replaceString_First(string& src, const string& key, const string& value);
  int replaceString_All(string& src, const string& key, const string& value);

  // convert a list of strings into a comma-separated string
  string commaSepStringFromStringList(const StringList& lst, const string& separator=", ");

  // assign a value to a pointed-to variable if
  // the pointer is not empty
  template<typename T>
  inline void assignIfNotNull(T* ptr, const T& val)
  {
    if (ptr != nullptr) *ptr = val;
  }

  // check whether a string is a valid email adress
  //
  // modifies the string in place (trimming)
  bool isValidEmailAddress(const string& email);


  // check whether an element is in a vector
  template<typename ElemType>
  bool isInVector(const vector<ElemType>& vec, const ElemType& el)
  {
    return (find(vec.begin(), vec.end(), el) != vec.end());
  }

  // erase all occurences of a value from a vector
  // and return the number of removed elements
  template<class T>
  int eraseAllOccurencesFromVector(vector<T>& vec, const T& val)
  {
    int oldSize = vec.size();
    vec.erase(std::remove(vec.begin(), vec.end(), val), vec.end());
    int newSize = vec.size();
    return oldSize - newSize;
  }

  // trim a string and make sure
  // it's not empty or too long
  bool trimAndCheckString(string& s, size_t maxLen = 0);

  // replace a section in a string with another string
  bool replaceStringSection(string& data, size_t startIdxToDelete, size_t endIdxToDelete, const string& replacement);

  // get a slice of a string delimited by two indices
  string getStringSlice(const string& s, size_t idxStart, size_t idxEnd);

  //----------------------------------------------------------------------------

  class ManagedMemory
  {
  public:
    ManagedMemory()
      :rawPtr{nullptr}, len{0}{}
    ManagedMemory(size_t _len);

    virtual ~ManagedMemory() {}

    void* get() const { return rawPtr; }
    char * get_c() const { return (char *)(rawPtr); }
    unsigned char * get_uc() const { return (unsigned char *)(rawPtr); }

    uint8_t byteAt(size_t idx) const;
    char charAt(size_t idx) const;

    size_t getSize() const { return len; }

    bool isValid() const { return ((len > 0) && (rawPtr != nullptr)); }

    string copyToString() const;

    virtual void shrink(size_t newSize) = 0;

  protected:
    void* rawPtr;
    size_t len;

    virtual void releaseMemory() = 0;
  };

  //----------------------------------------------------------------------------

  class ManagedBuffer : public ManagedMemory
  {
  public:
    ManagedBuffer() : ManagedMemory{}{}
    explicit ManagedBuffer(size_t _len);
    explicit ManagedBuffer(const string& src);
    virtual ~ManagedBuffer();

    // disable copy functions
    ManagedBuffer(const ManagedBuffer&) = delete; // no copy constructor
    ManagedBuffer& operator=(const ManagedBuffer&) = delete; // no copy assignment

    // move semantics
    ManagedBuffer(ManagedBuffer&& other); // move constructor
    virtual ManagedBuffer& operator=(ManagedBuffer&& other); // move assignment

    // create a copy
    static ManagedBuffer asCopy(const ManagedMemory& src);

    // size change
    virtual void shrink(size_t newSize) override;

  protected:
    virtual void releaseMemory() override;
  };

  //----------------------------------------------------------------------------

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
