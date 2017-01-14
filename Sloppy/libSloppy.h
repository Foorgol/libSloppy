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
    unsigned char * get_uc() const { return (unsigned char *)(rawPtr); }

    size_t getSize() const { return len; }

    bool isValid() { return ((len > 0) && (rawPtr != nullptr)); }

  protected:
    void* rawPtr;
    size_t len;

    virtual void releaseMemory() = 0;
  };

  //----------------------------------------------------------------------------

  class ManagedBuffer : public ManagedMemory
  {
  public:
    ManagedBuffer(size_t _len);
    virtual ~ManagedBuffer();

    // disable copy functions
    ManagedBuffer(const ManagedBuffer&) = delete; // no copy constructor
    ManagedBuffer& operator=(const ManagedBuffer&) = delete; // no copy assignment

    // move semantics
    ManagedBuffer(ManagedBuffer&& other); // move constructor
    virtual ManagedBuffer& operator=(ManagedBuffer&& other); // move assignment


  protected:
    virtual void releaseMemory() override;
  };

}

#endif
