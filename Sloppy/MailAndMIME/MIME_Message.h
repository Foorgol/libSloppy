/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2019  Volker Knollmann
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

#ifndef SLOPPY__MAIL_AND_MIME__MIME_MESSAGE_H
#define SLOPPY__MAIL_AND_MIME__MIME_MESSAGE_H

#include <string>
#include <memory>
#include <unordered_map>

#include "MailAndMIME.h"
#include "Header.h"
#include "Message.h"
#include "../String.h"

using namespace std;

namespace Sloppy
{
  namespace RFC2045
  {
    // Exceptions
    struct MalformedMessage {};
    struct MalformedHeader {};

    // forwards
    class ContentTypeHeader;
    class MessagePart;

    /** \brief An (incomplete) enum for different content types of
     * a MIME message
     */
    enum class ContentType
    {
      Unknown,    ///> Default
      Text_Html,  ///> text/html
      Text_Plain, ///> text/plain
      Multipart_FormData,   ///> form data
      // will grow over time
    };

    //----------------------------------------------------------------------------

    /** \brief Parses structured header field bodies
     *
     * A structured header field looks like "`fieldName: fieldValue; para1=val1; para2=val2`".
     *
     * Example:
     * `Content-Type: text/html; charset=UTF-8`
     */
    class StructuredHeaderBody
    {
    public:
      /** \brief Ctor from the clean-up header field body (cleaned-up = comments removed)
       */
      explicit StructuredHeaderBody(
          const estring& hdrBody   ///< the comment-free header field body to parse
          );

      /** \returns `true` if the header body contains a given parameter (e.g., `charset`)
       */
      bool hasParameter(const string& paraName) const;

      /** \returns the value of a given parameter or an empty string
       * if the parameter does not exist
       */
      estring getParameter(const string& paraName) const;

      /** \returns the value of the header body (everything up to the first semicolon)
       */
      inline estring getValue() const { return val; }

    private:
      estring val;   ///< the body's value
      unordered_map<estring, estring> params;   ///< a key-value map of all parameters in the header field
    };

    //----------------------------------------------------------------------------

    /** \brief A special class for parsing the `Content-Type` header of a message
     */
    class ContentTypeHeader
    {
    public:
      /** \brief Default ctor for an empty, invalid header
       *
       * This ctor is only provided for creating dummy ContentTypeHeader members
       * during instantiation of other classes.
       */
      ContentTypeHeader()
        :body{""}, type{ContentType::Unknown}, _isMultipart{false} {}

      /** \brief Ctor from the body of a `Content-Type` header field
       *
       * \throws RFC2045::MalformedHeader if the header body could not be parsed
       */
      explicit ContentTypeHeader(
          const string& hdrBody   ///< the header body to parse
          );

      /** \returns the content type defined in the header field
       */
      inline ContentType getType() const { return type; }


      /** \returns `true` if the header body contains a given parameter (e.g., `charset`)
       */
      bool hasParam(
          const string& pName   ///> the parameter's name
          ) const { return body.hasParameter(pName); }

      /** \returns the value of a given parameter or an empty string
       * if the parameter does not exist
       */
      estring getParam(
          const string& pName   ///> the parameter's name
          ) const { return body.getParameter(pName); }

      /** \returns `true` if this is a multipart message; `false` otherwise
       */
      bool isMultipart() const { return _isMultipart; }

    private:
      StructuredHeaderBody body;
      ContentType type;
      bool _isMultipart;
    };

    //----------------------------------------------------------------------------

    /** \brief A part of a (multipart) message
     *
     * \note The content of this part might contain sub-parts. Parsing sub-parts
     * is currently not supported.
     *
     */
    class MessagePart
    {
    public:
      /** \brief Ctor, stores the message part's content
       */
      MessagePart(
          const string& _content   ///< the message part's content
          )
        :content{_content} {}

      //inline bool hasSubParts() { return subParts.size() > 0; }

      /** \returns the message part's content
       */
      string getContent() const { return content; }

    protected:
      string content;   ///< the part's content
      //vector<MessagePart> subParts;
    };

    //----------------------------------------------------------------------------

    /** \brief Stores and parses a RFC-2045-compliant MIME message
     *
     * \note Support is limited to MIME version 1.0
     */
    class MIME_Message
    {
    public:
      /** \brief Ctor from an existing RFC822 message
       *
       * \throws RFC2045::MalformedMessage if the MIME version doesn't match or
       * if we don't have a Content-Type header or if parsing of the message parts failed.
       */
      MIME_Message(
          const RFC822::Message& inMsg,   ///< the message to parse as a MIME message
          bool skipMIMEVersionCheck=true  ///< if `true`, the MIME version header will not be checked
          );

      /** \returns the number of parts in the message
       */
      int getNumParts() const { return parts.size(); }

      /** \returns the content of a given part or an empty string if the part number was invalid
       */
      string getPart(size_t i   ///< the zero-based index of the part to retrieve
          ) const;

      /** \returns the ContentType of this MIME message
       */
      inline ContentType getContentType() { return ctHeader.getType(); }

    protected:
      /** \brief splits the message body into its MIME parts
       *
       * \throws RFC2045::MalformedMessage if parsing of the message parts failed.
       */
      void parseParts(
          const estring& body   ///< the raw messsage body to pars
          );

    private:
      ContentTypeHeader ctHeader;   ///< a representation of the Content-Type header of the message
      vector<MessagePart> parts;    ///< a list of all parts found in the message
    };

    //----------------------------------------------------------------------------


  }
}

#endif
