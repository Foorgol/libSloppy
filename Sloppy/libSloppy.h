#ifndef __LIBSLOPPY_SLOPPY_H
#define __LIBSLOPPY_SLOPPY_H

#include <string>
#include <vector>
#include <algorithm>

#include <boost/algorithm/string.hpp>

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
  inline bool trimAndCheckString(string& s, int maxLen = -1)
  {
    boost::trim(s);
    return (maxLen > 0) ? (!(s.empty() || (s.length() > maxLen))) : (!(s.empty()));
  }
}

#endif
