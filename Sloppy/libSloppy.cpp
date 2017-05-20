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

  int strArg(string& s, const string& arg)
  {
    constexpr int NotFound = 999999;
    int minArg = NotFound;

    // determine the lowest argument index
    regex re{R"(%(\d+))"};
    sregex_iterator begin{s.begin(), s.end(), re};
    for (auto it = begin; it != sregex_iterator{}; ++it)
    {
      int argIdx = stoi((*it)[1]);
      if (argIdx < 0) continue;
      if (argIdx < minArg) minArg = argIdx;
    }

    // search / replace the argument with the lowest index
    if (minArg != NotFound)
    {
      string key = "%" + to_string(minArg);
      return replaceString_All(s, key, arg);
    }

    return 0;
  }

  //----------------------------------------------------------------------------

  int strArg(string& s, int arg, int minLen, char fillChar)
  {
    // make the standard case easy and fast:
    // convert to string, replace, done
    string sArg = to_string(arg);
    if (sArg.size() >= minLen) return strArg(s, sArg);

    // if we don't meet the minimum length, apply
    // manual padding and consider the '-' character
    if (arg < 0) --minLen;
    sArg = to_string(abs(arg));
    int cnt = minLen - sArg.size();
    sArg = string(cnt, fillChar) + sArg;
    if (arg < 0) sArg = "-" + sArg;

    return strArg(s, sArg);
  }

  //----------------------------------------------------------------------------

  int strArg(string& s, double arg, int numDigits)
  {
    string fmt = "%";
    if (numDigits >=0) fmt += "." + to_string(numDigits);
    fmt += "f";

    return strArg<double>(s, arg, fmt);
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

      resultList.push_back(it->path().native());
    }
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



  //----------------------------------------------------------------------------

}
