
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
}
