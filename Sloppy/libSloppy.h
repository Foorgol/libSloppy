#ifndef __LIBSLOPPY_SLOPPY_H
#define __LIBSLOPPY_SLOPPY_H

#include <string>
#include <vector>

using namespace std;

namespace Sloppy
{
  using StringList = vector<string>;

  // split strings by delimiter string (easier to handle than Boost's function)
  void stringSplitter(StringList& target, const string& src, const string& delim);

  // replace substrings (similar to what Boost provides)
  bool replaceString_First(string& src, const string& key, const string& value);
  int replaceString_All(string& src, const string& key, const string& value);

  // convert a list of strings into a comma-separated string
  string commaSepStringFromStringList(const StringList& lst, const string& separator=", ");
}

#endif
