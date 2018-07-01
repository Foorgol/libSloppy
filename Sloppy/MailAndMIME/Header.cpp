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

#include <algorithm>

#include <boost/algorithm/string.hpp>

#include "Header.h"

namespace Sloppy
{
  namespace RFC822
  {

    Header::Header(const estring& rawHeaderData)
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
      vector<estring> hdrLines = rawHeaderData.split(sCRLF, true, false);

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
      for (const estring& f : hdrLines)
      {
        size_t colonPos = f.find(':');
        if (colonPos == string::npos)
        {
          throw MalformedHeader();   // every field needs a colon as delimiter
        }

        estring fieldName = f.slice(0, colonPos - 1);
        estring fieldBody = f.slice(colonPos + 1);

        fields.push_back(HeaderField{fieldName, fieldBody});
      }
    }

    //----------------------------------------------------------------------------

    vector<estring> Header::getRawFieldBody(const string& fieldName) const
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

    string Header::getFieldBody_Simple(const string& fieldName) const
    {
      auto it = find_if(fields.begin(), fields.end(), [&fieldName](const HeaderField& f) {
        return f == fieldName;
      });
      if (it == fields.end()) return "";

      return it->getBody();
    }

    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    HeaderField::HeaderField(const string& fName, const string& fBody)
      : fieldName{fName}, fieldBody_raw{fBody}, fieldBody{}
    {
      // trim field names on both sides
      fieldName.trim();

      // trim the body only on the left side. This removes a possible
      // space between the colon and the field body
      fieldBody_raw.trimLeft();

      // since field names are case-insensitive, we store
      // all field names as lower case values
      fieldName.toLower();

      // erase comments from the field body
      //
      // FIX: what to do if after removing the comments the remaining
      // body is empty or only consisting of white spaces?
      fieldBody = removeCommentsFromBody(fieldBody_raw);
    }

    //----------------------------------------------------------------------------

    bool HeaderField::operator ==(const string& fName) const
    {
      estring fn{fName};
      fn.toLower();
      return (fieldName == fn);
    }

    //----------------------------------------------------------------------------

    bool HeaderField::operator !=(const string& fName) const
    {
      estring fn{fName};
      fn.toLower();
      return (fieldName != fn);
    }

    //----------------------------------------------------------------------------

    estring HeaderField::removeCommentsFromBody(const estring& rawBody) const
    {
      // a little helper function that determines whether a character
      // in rawBody is escaped (preceded by '\') or not
      auto pred_isEscaped = [&rawBody](size_t pos)
      {
        if (pos == 0) return false;
        return rawBody[pos-1] == '\\';
      };

      // a placeholder for the result
      string result;

      // a lambda for a recursive search of (nested) comments
      function<size_t(size_t, int)> searchComment_recursion = [&](size_t startPos, int lvl) -> size_t
      {
        size_t i = startPos;
        if (lvl > 0) ++i;   // if we are at levels > 0, startPos is already the first valid opening bracket

        // search for the associated closing bracket or
        // dive recursively into a nested comment
        while (i < rawBody.length())
        {
          // test for a valid closing bracket
          if ((rawBody[i] == ')') && (!(pred_isEscaped(i))))
          {
            if (lvl == 0) throw MalformedHeader(); // ERROR: found closing bracket without opening bracket
            return i;  // return the index of the closing bracket
          }

          // test for a (maybe nested) comment
          if ((rawBody[i] == '(') && (!(pred_isEscaped(i))))
          {
            // i is the index of the opening bracket of a comment
            // ==> search for the position of the associated closing bracket
            size_t cmtEnd = searchComment_recursion(i, lvl+1);

            if (cmtEnd == string::npos) return string::npos; // ERROR, causes throwing of MalformedHeader by the caller

            if (lvl == 0)   // we found the end of the outmost comment
            {
              // copy everything from the start index up to but
              // not including the opening bracket
              if ((i != startPos) && (i > 0)) result += rawBody.slice(startPos, i - 1);

              // the next non-comment text chunk starts after
              // the outer closing bracket position
              //
              // FIX: this does not work if we have directly
              // adjacent comments like "....(comment1)(comment2)...."
              //
              startPos = cmtEnd + 1;
            }

            // fast-forward to the end of the nested comment
            i = cmtEnd;
          }

          ++i;
        }

        if (lvl != 0) throw MalformedHeader();  // we should never reach the end of the string while we're in a comment (--> lvl > 0)

        // add the remaining part of the source string, in case the source
        // doesn't end with a comment
        result += rawBody.substr(startPos);

        return 0;  // 0 = dummy value for a regular end of the recursion
      };


      // find all comments and add the non-comment-part of the field body
      // directly to 'result'
      size_t rc = searchComment_recursion(0, 0);
      if (rc == string::npos)
      {
        throw MalformedHeader{};
      }

      return result;
    }

    //----------------------------------------------------------------------------

  }
}
