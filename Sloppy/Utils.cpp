/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2018  Volker Knollmann
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
#include <tuple>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include "Utils.h"
#include "json.hpp"
#include "ManagedFileDescriptor.h"
#include "Memory.h"

namespace bfs = boost::filesystem;
using json = nlohmann::json;

namespace Sloppy
{
  bool isValidEmailAddress(const string& email)
  {
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

  string json2String(const nlohmann::json& jv, int numDigits)
  {
    // helper variables for floating point conversion
    string fmt = "%";
    char buf[100];

    switch (jv.type())
    {
    case json::value_t::boolean:
      return to_string(jv.get<bool>());

    case json::value_t::number_unsigned:
      return to_string(jv.get<unsigned long>());

    case json::value_t::number_integer:
      return to_string(jv.get<long>());

    case json::value_t::number_float:
      if (numDigits >=0) fmt += "." + to_string(numDigits);
      fmt += "f";

      snprintf(buf, 100, fmt.c_str(), jv.get<double>());
      return string{buf};

    case json::value_t::string:
      return jv;

    case json::value_t::null:
      return "";

    default:
      throw std::invalid_argument("json2string: the JSON object is of an invalid type");
    }

    return "!!! JSON conversion error !!!";    // we should never get here
  }

  //----------------------------------------------------------------------------

#ifndef WIN32

  BiDirPipeEnd::BiDirPipeEnd(int _fdRead, int _fdWrite)
    :fdRead{_fdRead}, fdWrite{_fdWrite}
  {

  }

#endif

  //----------------------------------------------------------------------------

#ifndef WIN32

  BiDirPipeEnd& BiDirPipeEnd::operator=(BiDirPipeEnd&& other) noexcept
  {
    fdRead = std::move(other.fdRead);
    fdWrite = std::move(other.fdWrite);

    return *this;
  }

#endif

  //----------------------------------------------------------------------------

#ifndef WIN32

  BiDirPipeEnd::BiDirPipeEnd(BiDirPipeEnd&& other) noexcept
    :fdRead{std::move(other.fdRead)}, fdWrite{std::move(other.fdWrite)}
  {

  }

#endif

  //----------------------------------------------------------------------------

#ifndef WIN32

  bool BiDirPipeEnd::blockingWrite(const string& data)
  {
    return fdWrite.blockingWrite(data);
  }

#endif

  //----------------------------------------------------------------------------

#ifndef WIN32

  bool BiDirPipeEnd::blockingWrite(const MemView& data)
  {
    return fdWrite.blockingWrite(data);
  }

#endif

  //----------------------------------------------------------------------------

#ifndef WIN32

  bool BiDirPipeEnd::blockingWrite(const char* ptr, size_t len)
  {
    return fdWrite.blockingWrite(ptr, len);
  }

#endif

  //----------------------------------------------------------------------------

#ifndef WIN32

  MemArray BiDirPipeEnd::blockingRead(size_t minLen, size_t maxLen, size_t timeout_ms)
  {
    return fdRead.blockingRead(minLen, maxLen, timeout_ms);
  }

#endif

  //----------------------------------------------------------------------------

#ifndef WIN32

  MemArray BiDirPipeEnd::blockingRead_FixedSize(size_t expectedLen, size_t timeout_ms)
  {
    return fdRead.blockingRead_FixedSize(expectedLen, timeout_ms);
  }

#endif

  //----------------------------------------------------------------------------

#ifndef WIN32

  void BiDirPipeEnd::close()
  {
    fdRead.close();
    fdWrite.close();
  }

#endif

  //----------------------------------------------------------------------------

#ifndef WIN32

  pair<BiDirPipeEnd, BiDirPipeEnd> createBirectionalPipe()
  {
    // create two pipes
    int fd_pipe1[2];
    pipe(fd_pipe1);
    int fd_pipe2[2];
    pipe(fd_pipe2);

    // create two pipe ends with "crossed" descriptors
    BiDirPipeEnd e1{fd_pipe1[0], fd_pipe2[1]};
    BiDirPipeEnd e2{fd_pipe2[0], fd_pipe1[1]};

    return make_pair(std::move(e1), std::move(e2));
  }

#endif

  //----------------------------------------------------------------------------

#ifndef WIN32

  pair<ManagedFileDescriptor, ManagedFileDescriptor> createSimplePipe()
  {
    // create the pipe
    int fd_pipe[2];
    pipe(fd_pipe);

    // wrap the descriptors into ManagedFileDescriptor instances
    ManagedFileDescriptor fdRead{fd_pipe[0]};
    ManagedFileDescriptor fdWrite{fd_pipe[1]};

    return make_pair(std::move(fdRead), std::move(fdWrite));
  }

#endif

  //----------------------------------------------------------------------------



  //----------------------------------------------------------------------------

}
