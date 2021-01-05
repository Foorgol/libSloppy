/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2021  Volker Knollmann
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

#include <sys/stat.h>                       // for stat, S_ISDIR, S_ISREG
#include <unistd.h>                         // for pipe, getcwd
#include <boost/algorithm/string/trim.hpp>  // for trim
#include <cstdint>                          // for int64_t, uint8_t
#include <cstdio>                           // for size_t, snprintf
#include <filesystem>                       // for directory_iterator, direc...
#include <regex>                            // for regex_match, match_result...
#include <stdexcept>                        // for invalid_argument

#include "ManagedFileDescriptor.h"          // for ManagedFileDescriptor
#include "Memory.h"                         // for MemArray, MemView
#include "json.hpp"                         // for json, iter_impl, basic_json

#include "Utils.h"

using json = nlohmann::json;
using namespace std;

namespace std
{
  string to_string(const char* cString)
  {
    return string{cString};
  }

  const string& to_string(const string& s)
  {
    return s;
  }

}

namespace Sloppy
{
  bool isValidEmailAddress(const string& email)
  {
    regex reEmail{R"(^(?=[A-Z0-9][A-Z0-9@._%+-]{5,253}+$)[A-Z0-9._%+-]{1,64}+@(?:(?=[A-Z0-9-]{1,63}+\.)[A-Z0-9]++(?:-[A-Z0-9]++)*+\.){1,8}+[A-Z]{2,63}+$)",
                 regex::icase};

    string e{email};
    return regex_match(e, reEmail);
  }

  //----------------------------------------------------------------------------

  bool trimAndCheckString(string& s, size_t maxLen)
  {
    boost::trim(s);
    return (maxLen > 0) ? (!(s.empty() || (s.length() > maxLen))) : (!(s.empty()));
  }

  //----------------------------------------------------------------------------

  void getAllFilesInDirTree_Recursion(const std::filesystem::path & basePath, StringList& resultList, bool includeDirNameInList)
  {
    
    for (std::filesystem::directory_iterator it{basePath}; it != std::filesystem::directory_iterator{}; ++it)
    {
      if (std::filesystem::is_directory(it->status()))
      {
        getAllFilesInDirTree_Recursion(it->path(), resultList, includeDirNameInList);
        
        if (!includeDirNameInList) continue;
      }
      
      resultList.push_back(it->path().string());
    }
  }
  
  //----------------------------------------------------------------------------
  
  StringList getAllFilesInDirTree(const string& baseDir, bool includeDirNameInList)
  {
    std::filesystem::path root{baseDir};
    if (!(std::filesystem::exists(root))) return StringList{};

    StringList result;
    getAllFilesInDirTree_Recursion(root, result, includeDirNameInList);

    return result;
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
      return to_string(jv.get<unsigned long long>());

    case json::value_t::number_integer:
      return to_string(jv.get<int64_t>());

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

  bool jsonObjectHasKey(const nlohmann::json& js, const string& key, nlohmann::json::value_t requiredValueType)
  {
    if (!js.is_object()) return false;

    auto it = js.find(key);
    if (it == js.end()) return false;

    return (it->type() == requiredValueType);
  }

  //----------------------------------------------------------------------------

  bool jsonObjectHasKey(const nlohmann::json& js, const string& key)
  {
    if (!js.is_object()) return false;
    auto it = js.find(key);
    return (it != js.end());
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
