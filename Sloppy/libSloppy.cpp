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
#include <iostream>
#include <cstdio>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include "libSloppy.h"

namespace bfs = boost::filesystem;

namespace Sloppy
{
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

  StringList getAllFilesInDirTree(const string& baseDir, bool includeDirNameInList)
  {
    bfs::path root{baseDir};
    if (!(bfs::exists(root))) return StringList{};

    StringList result;
    getAllFilesInDirTree_Recursion(root, result, includeDirNameInList);

    return result;
  }

  //----------------------------------------------------------------------------

  void getAllFilesInDirTree_Recursion(const bfs::path & basePath, StringList& resultList, bool includeDirNameInList)
  {

    for (bfs::directory_iterator it{basePath}; it != bfs::directory_iterator{}; ++it)
    {
      if (bfs::is_directory(it->status()))
      {
        getAllFilesInDirTree_Recursion(it->path(), resultList, includeDirNameInList);

        if (!includeDirNameInList) continue;
      }

      resultList.push_back(it->path().string());
    }
  }

  //----------------------------------------------------------------------------

#ifndef WIN32
  string getCurrentWorkDir()
  {
    char buf[1000];
    getcwd(buf, 1000);
    return string{buf};
  }
#endif

  //----------------------------------------------------------------------------

#ifndef WIN32
  bool isFile(const string& fName)
  {
    struct stat statBuf;
    if (stat(fName.c_str(), &statBuf) != 0) return false;   // name doesn't exist
    return (S_ISREG(statBuf.st_mode));
  }
#endif

  //----------------------------------------------------------------------------

#ifndef WIN32
  bool isDirectory(const string& dirName)
  {
    struct stat statBuf;
    if (stat(dirName.c_str(), &statBuf) != 0) return false;   // name doesn't exist
    return (S_ISDIR(statBuf.st_mode));
  }
#endif

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

  ManagedBuffer ManagedBuffer::asCopy(const ManagedMemory& src)
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

  bool isInt(const string& s)
  {
    //
    // looks clumsy but I believe it's faster
    // than lexical_cast or regex
    //
    // and stoi converts "5xy" to 5, for instance
    //

    if (s.empty()) return false;

    const char* p = s.c_str();
    bool isSigned = (*p == '-');
    if (isSigned && (s.size() < 2)) return false;

    if (isSigned) ++p;
    while (true)
    {
      char c = *p;
      if (c == 0) return true;
      if ((c < '0') || (c > '9')) return false;
      ++p;
    }
  }

  //----------------------------------------------------------------------------

  bool isDouble(const string& s)
  {
    //
    // looks clumsy but I believe it's faster
    // than lexical_cast or regex
    //
    // and stoi converts "5xy" to 5, for instance
    //

    if (s.empty()) return false;

    const char* p = s.c_str();
    bool isSigned = (*p == '-');
    if (isSigned && (s.size() < 2)) return false;

    if (isSigned) ++p;
    while (true)
    {
      char c = *p;
      if (c == 0) return true;
      if (((c < '0') || (c > '9')) && (c != '.')) return false;
      ++p;
    }
  }


  //----------------------------------------------------------------------------



  //----------------------------------------------------------------------------

}
