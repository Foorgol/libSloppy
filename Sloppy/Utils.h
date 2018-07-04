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

#ifndef __LIBSLOPPY_UTILS_H
#define __LIBSLOPPY_UTILS_H

#include <vector>
#include <algorithm>

// we include some special file functions for
// non-Windows builds only
#ifndef WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "String.h"
#include "../json_fwd.hpp"

using namespace std;

// a forward declaration
namespace boost { namespace filesystem { class path; }}

namespace Sloppy
{
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

  // find all files in a directors including sub-folders
  StringList getAllFilesInDirTree(const string& baseDir, bool includeDirNameInList=false);
  void getAllFilesInDirTree_Recursion(const boost::filesystem::path& basePath, StringList& resultList, bool includeDirNameInList);

  // we include some special file functions for
  // non-Windows builds only
#ifndef WIN32
  string getCurrentWorkDir();
  bool isFile(const string& fName);
  bool isDirectory(const string& dirName);
#endif

  /** \brief Converts a JSON-value to string, regardless of the actual inner JSON type
   *
   * Is designed for single JSON values, not arrays etc. This is not a replacement
   * for serializing via dump().
   */
  string json2String(
          const nlohmann::json& jv,   ///< the JSON value that shall be converted
          const string& defVal = string{}   ///< a default value if none of the conversions worked
          );

}

#endif
