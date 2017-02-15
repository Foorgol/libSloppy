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

#include <regex>

#include <boost/algorithm/string.hpp>

#include "libSloppy.h"

namespace Sloppy
{
  void stringSplitter(Sloppy::StringList& target, const std::__cxx11::string& src, const std::__cxx11::string& delim, bool trimStrings)
  {
    // split the source up at 'delim' positions
    //
    // note: boost::split and <regex> don't work well here.
    // regex are complicated because we can't rely on the last
    // segment ending with the delimiter token
    size_t nextStartPos = 0;
    while (nextStartPos < src.length())
    {
      size_t nextDelimPos = src.find(delim, nextStartPos);
      if (nextDelimPos == string::npos) break;

      string s = src.substr(nextStartPos, nextDelimPos - nextStartPos);
      if (trimStrings) boost::trim(s);
      target.push_back(s);
      nextStartPos = nextDelimPos + delim.length();
    }
    if (nextStartPos < src.length())
    {
      string s = src.substr(nextStartPos, src.length() - nextStartPos);
      if (trimStrings) boost::trim(s);
      target.push_back(s);
    }
  }

  //----------------------------------------------------------------------------

  bool replaceString_First(string& src, const string& key, const string& value)
  {
    if (src.empty()) return false;
    if (key.empty()) return false;

    // find first occurence of "key"
    size_t startPos = src.find(key);
    if (startPos == string::npos) return false;

    // replace this first occurence with the new value
    src.replace(startPos, key.length(), value);
    return true;
  }

  //----------------------------------------------------------------------------

  int replaceString_All(string& src, const string& key, const string& value)
  {
    int cnt = 0;
    while (replaceString_First(src, key, value)) ++cnt;

    return cnt;
  }

  //----------------------------------------------------------------------------

  string commaSepStringFromStringList(const StringList& lst, const string &separator)
  {
    string result;

    for (size_t i=0; i<lst.size(); i++)
    {
      const string& v = lst.at(i);
      if (i > 0)
      {
        result += separator;
      }
      result += v;
    }

    return result;
  }

  //----------------------------------------------------------------------------

  bool isValidEmailAddress(const string& email)
  {
    //
    // Well, this is weird. Using the case-insensitive mode
    // of a regex doesn't work as expected. Example: the
    // simple pattern ^[A-Z]+ used with regex::icase does match
    // "ABC", "aaa" but not "abc". I would expect to match "abc"
    // as well. Hmmm....
    //
    // So for the purpose of "simple" regex, I only use the
    // character class A-Z and convert the email address to
    // uppercase first. I hoped the uppercase conversion would
    // not be necessary, but the simple test explained above
    // proved me wrong.
    //
    // The regex is taken from
    // http://www.regular-expressions.info/email.html
    //
    regex reEmail{R"(^(?=[A-Z0-9][A-Z0-9@._%+-]{5,253}+$)[A-Z0-9._%+-]{1,64}+@(?:(?=[A-Z0-9-]{1,63}+\.)[A-Z0-9]++(?:-[A-Z0-9]++)*+\.){1,8}+[A-Z]{2,63}+$)",
                 regex::icase};

    string e{email};
    boost::to_upper(e);
    return regex_match(e, reEmail);
  }

  //----------------------------------------------------------------------------

  bool trimAndCheckString(string& s, size_t maxLen)
  {
    boost::trim(s);
    return (maxLen > 0) ? (!(s.empty() || (s.length() > maxLen))) : (!(s.empty()));
  }

  //----------------------------------------------------------------------------

  bool replaceStringSection(string& data, size_t startIdxToDelete, size_t endIdxToDelete, const string& replacement)
  {
    if (endIdxToDelete < startIdxToDelete) return false;

    if ((startIdxToDelete > (data.size() - 1)) || (endIdxToDelete > (data.size() - 1))) return false;

    string part1;
    if (startIdxToDelete > 0)
    {
      part1 = data.substr(0, startIdxToDelete);
    }

    string part2;
    if (endIdxToDelete < (data.size() - 1))
    {
      part2 = data.substr(endIdxToDelete + 1);
    }

    data = part1 + replacement + part2;

    return true;
  }

  //----------------------------------------------------------------------------

  string getStringSlice(const string& s, size_t idxStart, size_t idxEnd)
  {
    if (idxEnd < idxStart) return "";

    return s.substr(idxStart, idxEnd - idxStart + 1);
  }

  //----------------------------------------------------------------------------
  //----------------------------------------------------------------------------
  //----------------------------------------------------------------------------

  ManagedMemory::ManagedMemory(size_t _len)
      :rawPtr{nullptr}, len{_len}
  {
    if (len == 0)
    {
      throw invalid_argument("Cannot allocate zero bytes of memory!");
    }
  }

  //----------------------------------------------------------------------------

  uint8_t ManagedMemory::byteAt(size_t idx) const
  {
    if (idx >= len)
    {
      throw range_error{"invalid index in ManagedMemory access function!"};
    }

    uint8_t* base = (uint8_t *)rawPtr;
    return base[idx];
  }

  //----------------------------------------------------------------------------

  char ManagedMemory::charAt(size_t idx) const
  {
    if (idx >= len)
    {
      throw range_error{"invalid index in ManagedMemory access function!"};
    }

    char* base = (char *)rawPtr;
    return base[idx];
  }

  //----------------------------------------------------------------------------

  string ManagedMemory::copyToString() const
  {
    if (!(isValid())) return string{};

    return string{static_cast<char*>(rawPtr), static_cast<char*>(rawPtr) + len};
  }

  //----------------------------------------------------------------------------
  //----------------------------------------------------------------------------
  //----------------------------------------------------------------------------

  ManagedBuffer::ManagedBuffer(size_t _len)
    :ManagedMemory{_len}
  {
    rawPtr = malloc(_len);

    // was the allocation successful?
    if (rawPtr == nullptr)
    {
      throw runtime_error{"out of heap"};
    }
  }

  //----------------------------------------------------------------------------

  ManagedBuffer::ManagedBuffer(const string& src)
    :ManagedBuffer{src.size()}
  {
    memcpy(rawPtr, src.c_str(), len);
  }

  //----------------------------------------------------------------------------

  ManagedBuffer::~ManagedBuffer()
  {
    releaseMemory();
  }

  //----------------------------------------------------------------------------

  ManagedBuffer::ManagedBuffer(ManagedBuffer&& other)
  {
    *this = std::move(other);
  }

  //----------------------------------------------------------------------------

  ManagedBuffer& ManagedBuffer::operator=(ManagedBuffer&& other)
  {
    releaseMemory();

    rawPtr = other.rawPtr;
    len = other.len;

    other.rawPtr = nullptr;
    other.len = 0;

    return *this;
  }

  //----------------------------------------------------------------------------

  ManagedBuffer ManagedBuffer::asCopy(const ManagedBuffer& src)
  {
    if (!(src.isValid())) return ManagedBuffer{};

    ManagedBuffer cpy{src.getSize()};
    memcpy(cpy.get(), src.get(), src.getSize());

    return std::move(cpy);
  }

  //----------------------------------------------------------------------------

  void ManagedBuffer::shrink(size_t newSize)
  {
    // range check for the new size
    if ((newSize == 0) || (newSize >= len)) return;

    // allocate mem for the new size
    void* newMem = malloc(newSize);
    if (newMem == nullptr)
    {
      throw runtime_error{"out of heap"};
    }

    // copy the data over
    memcpy(newMem, rawPtr, newSize);

    // release the old memory and store the new one
    releaseMemory();
    rawPtr = newMem;
    len = newSize;
  }

  //----------------------------------------------------------------------------

  void ManagedBuffer::releaseMemory()
  {
    if (rawPtr != nullptr) free(rawPtr);
    len = 0;
  }

  //----------------------------------------------------------------------------

  constexpr int ManagedFileDescriptor::DefaultReadWait_ms;

  ManagedFileDescriptor::ManagedFileDescriptor(int _fd)
    :fd{_fd}, readBuf{nullptr}, st{State::Idle}
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

    if (st != State::Closed) ::close(fd);

    if (readBuf != nullptr) delete readBuf;
  }

  //----------------------------------------------------------------------------

  bool ManagedFileDescriptor::blockingWrite(const string &data)
  {
    // wait for the fd to become available
    lock_guard<mutex> lockFd{fdMutex};

    // just to be sure: check the state
    if (st != State::Idle) return false;

    // do the actual write
    st = State::Writing;
    int n = write(fd, data.c_str(), data.size());
    st = State::Idle;

    if (n < 0)
    {
      throw IOError{};
    }

    return (((size_t) n) == data.size());
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
          throw ReadTimeout{result};
        }

        // wait for data to become available
        size_t remainingTime = timeout_ms - elapsedTime;
        timeval tv;
        tv.tv_sec = remainingTime / 1000;
        tv.tv_usec = (remainingTime % 1000) * 1000;
        fd_set readFd;
        FD_ZERO(&readFd);
        FD_SET(fd, &readFd);
        int retVal = select(fd+1, &readFd, nullptr, nullptr, &tv);

        // evaluate the result
        if (retVal < 0)
        {
          throw IOError{};
        }
        if (retVal == 0)
        {
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

      if (result.size() >= minLen) break;
    }

    st = State::Idle;

    return result;
  }

  //----------------------------------------------------------------------------

  ManagedFileDescriptor::State ManagedFileDescriptor::getState()
  {
    return st;
  }



  //----------------------------------------------------------------------------

}
