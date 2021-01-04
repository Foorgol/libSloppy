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

#ifndef SLOPPY__MAIL_AND_MIME__HEADER_H
#define SLOPPY__MAIL_AND_MIME__HEADER_H

#include <string>
#include <vector>

#include "MailAndMIME.h"
#include "../String.h"

namespace Sloppy
{
  namespace RFC822
  {
    // exception
    struct MalformedHeader {};

    //----------------------------------------------------------------------------

    /** \brief A single header field in the header of an RFC 882 compliant email message
     */
    class HeaderField
    {
    public:
      /** \brief Ctor from the the field name (everything left of the colon) and the field content (everythin right of the colon)
       *
       * Assumes that the headers are already unfolded.
       */
      HeaderField(
          const std::string& fName,   ///< the header's name
          const std::string& fBody    ///< the header field's content
          );

      /** \returns `true` if the header field's name matches a given string.
       *
       * The comparision is case-insensitive.
       */
      bool operator ==(
          const std::string& fName   ///< the string to check the field's name against
          ) const;

      /** \returns `true` if the header field's name NOT matches a given string.
       *
       * The comparision is case-insensitive.
       */
      bool operator !=(
          const std::string& fName   ///< the string to check the field's name against
          ) const;

      /** \returns the raw field content including comments etc.
       */
      inline estring getRawBody() const
      {
        return fieldBody_raw;
      }

      /** \returns the pure field content with comments removed
       */
      inline estring getBody() const
      {
        return fieldBody;
      }

      /** \brief parses a raw body string and returns a version without comments.
       *
       * \returns a field body without comments.
       */
      estring removeCommentsFromBody(
          const estring& rawBody   ///< the raw body content to remove the comments from
          ) const;

    private:
      estring fieldName;       ///< the field's name (the part left of the colon)
      estring fieldBody_raw;   ///< the field's raw body (the part right of the colon)
      estring fieldBody;       ///< the field's body without comments
    };

    //----------------------------------------------------------------------------

    /** \brief Represents the full header of a RFC 822 compliant email message
     */
    class Header
    {
    public:
      /** \brief Default ctor for an empty header object
       */
      explicit Header() {}

      /** \brief Ctor from raw header data, e.g. as delivered by SMTP
       *
       * The terminating CR-LF-CR-LF should not be part of the input.
       *
       * \throws MalformedHeader if the header data was empty or could not be parsed.
       *
       */
      explicit Header(
          const estring& rawHeaderData   ///< the raw email header up to but not including the CR-LF-CR-LF
          );

      /** \brief Collects the contents of all instances of a header field with a given name.
       *
       * Mail messages can contain multiple instances of a header (e.g., "Received"). This
       * method returns the header bodies of all instances as a list.
       *
       * The header name is treated case-insensitve.
       *
       * \returns a list of all field bodies for a given header name
       */
      std::vector<estring> getRawFieldBody(
          const std::string& fieldName   ///< the name of the header field to retrieve
          ) const;

      /** \returns `true` if the message header contains a field with a given name
       *
       * The header field's name is treated case-insensitive.
       */
      bool hasField(
          const std::string& fieldName   ///< the name of the header field to search for
          ) const;

      /** \returns the number of fields in the header.
       *
       * Folded header lines are only counted as one.
       *
       * Multiple instances of the same header field (e.g., "Received")
       * are all counted individually (e.g. 3 x "Received" ==> 3 lines)
       */
      inline int getFieldCount() const
      {
        return fields.size();
      }

      /** \brief Searches for the first occurence of a header with a given name and returns it's raw content
       *
       * The header field's name is treated case-insensitive.
       *
       * \returns the raw body (incl. comments) of the requested header field or "" if the header field doesn't exist
       */
      std::string getRawFieldBody_Simple(const std::string& fieldName) const;

      /** \brief Searches for the first occurence of a header with a given name and returns it's content
       *
       * The header field's name is treated case-insensitive.
       *
       * \returns the pure body without comments of the requested header field or "" if the header field doesn't exist
       */
      std::string getFieldBody_Simple(const std::string& fieldName) const;

    private:
      std::vector<HeaderField> fields;   ///< a list of all header fields in this header
    };

  }
}
#endif
