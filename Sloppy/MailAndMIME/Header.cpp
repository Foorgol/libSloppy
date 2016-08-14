#include <algorithm>

#include <boost/algorithm/string.hpp>

#include "../../libSloppy.h"
#include "Header.h"

namespace Sloppy
{
  namespace RFC822
  {

    Header::Header(const string& rawHeaderData)
    {
      if (rawHeaderData.empty())
      {
        throw MalformedHeader();
      }

      // we need at least one colon
      if (rawHeaderData.find(':') == string::npos)
      {
        throw MalformedHeader();
      }

      // split the headers up at CRLF positions
      //
      // note: boost::split and <regex> don't work well here.
      // regex are complicated because we can't rely on the last
      // line ending with the delimiter token CRLF
      StringList hdrLines;
      stringSplitter(hdrLines, rawHeaderData, sCRLF);

      // unfold header lines
      auto it = hdrLines.begin();
      while (it != hdrLines.end())
      {
        char startChar = (*it)[0];
        if ((startChar == 0x20) || (startChar == 0x09))
        {
          // merge this line with the previous one
          if (it == hdrLines.begin())
          {
            throw MalformedHeader();
          }
          auto prevLine = it - 1;
          *prevLine = *prevLine + *it;
          hdrLines.erase(it);
        } else {
          ++it;
        }
      }

      // split header lines in field-name and field-body
      for (const string& f : hdrLines)
      {
        size_t colonPos = f.find(':');
        if (colonPos == string::npos)
        {
          throw MalformedHeader();   // every field needs a colon as delimiter
        }

        string fieldName = f.substr(0, colonPos);
        string fieldBody = f.substr(colonPos + 1, f.length() - colonPos);

        fields.push_back(HeaderField{fieldName, fieldBody});
      }
    }

    //----------------------------------------------------------------------------

    StringList Header::getRawFieldBody(const string& fieldName) const
    {
      StringList result;
      for_each(fields.begin(), fields.end(), [&](const HeaderField& f) {
        if (f == fieldName)
        {
          result.push_back(f.getRawBody());
        }
      });

      return result;
    }

    //----------------------------------------------------------------------------

    bool Header::hasField(const string& fieldName) const
    {
      return any_of(fields.begin(), fields.end(), [&fieldName](const HeaderField& f) {
        return f == fieldName;
      });
    }

    //----------------------------------------------------------------------------

    string Header::getRawFieldBody_Simple(const string& fieldName) const
    {
      auto it = find_if(fields.begin(), fields.end(), [&fieldName](const HeaderField& f) {
        return f == fieldName;
      });
      if (it == fields.end()) return "";

      return it->getRawBody();
    }

    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    HeaderField::HeaderField(const string& fName, const string& fBody)
      : fieldName{fName}, fieldBody{fBody}
    {
      // trim field names on both sides
      boost::trim(fieldName);

      // trim the body only on the left side. This is a possible
      // space between the colon and the field body
      boost::trim_left(fieldBody);

      // since field names are case-insensitive, we store
      // all field names as lower case values
      boost::to_lower(fieldName);
    }

    //----------------------------------------------------------------------------

  }
}
