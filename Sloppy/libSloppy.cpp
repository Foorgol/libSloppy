
#include "libSloppy.h"

namespace Sloppy
{
  void stringSplitter(Sloppy::StringList& target, const std::__cxx11::string& src, const std::__cxx11::string& delim)
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

      target.push_back(src.substr(nextStartPos, nextDelimPos - nextStartPos));
      nextStartPos = nextDelimPos + delim.length();
    }
    if (nextStartPos < src.length())
    {
      target.push_back(src.substr(nextStartPos, src.length() - nextStartPos));
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
    while (replaceString_First(src, key, value));
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


}
