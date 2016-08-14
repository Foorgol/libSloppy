#ifndef __LIBSLOPPY_SLOPPY_H
#define __LIBSLOPPY_SLOPPY_H

#include <string>
#include <vector>

using namespace std;

namespace Sloppy
{
  using StringList = vector<string>;

  void stringSplitter(StringList& target, const string& src, const string& delim);
}

#endif
