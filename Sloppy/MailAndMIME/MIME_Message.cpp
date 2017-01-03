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

#include <regex>

#include <boost/algorithm/string.hpp>

#include "MIME_Message.h"

namespace Sloppy
{
  namespace RFC2045
  {

    MIME_Message::MIME_Message(const RFC822::Message& inMsg, bool skipMIMEVersionCheck)
    {
      RFC822::Header* hdr = inMsg.getHeaderPtr();

      // check the correct MIME version, if requested
      if (!skipMIMEVersionCheck)
      {
        string v = hdr->getFieldBody_Simple("MIME-Version");
        if (v != "1.0")
        {
          throw RFC2045::MalformedMessage();
        }
      }

      // make sure we have a Content-Type header
      string ct = hdr->getFieldBody_Simple("Content-Type");
      if (ct.empty())
      {
        throw RFC2045::MalformedMessage();
      }
      ctHeader = ContentTypeHeader{ct};

      // parse the message parts
      parseParts(inMsg.getBodyData());
    }

    //----------------------------------------------------------------------------

    string MIME_Message::getPart(int i) const
    {
      return (i >= static_cast<int>(parts.size())) ? string{} : parts[i].getContent();
    }

    //----------------------------------------------------------------------------

    void MIME_Message::parseParts(const string& body)
    {
      if (!ctHeader.isMultipart())
      {
        MessagePart p{body};
        parts.push_back(p);
        return;
      }

      // we have a multipart message. find the boundary strings
      string boundary = ctHeader.getParam("boundary");
      if (boundary.empty())
      {
        throw RFC2045::MalformedMessage();
      }

      string sectionDelimiter = "--" + boundary;
      string partEndTag = sectionDelimiter + "--";

      // find the end tag of the multipart message
      size_t endPos = body.find(partEndTag, 0);
      if (endPos == string::npos)
      {
        throw RFC2045::MalformedMessage();
      }

      // find one part after the other
      size_t curSectionStartPos = 0;
      while ((curSectionStartPos < endPos) && (curSectionStartPos != string::npos))
      {
        size_t nextSectionStartPos = body.find(sectionDelimiter, curSectionStartPos + 1);

        // the next data block incl. head delimiter starts at
        //    curPos
        // and the last character is at "nextPos - 1"

        size_t lastSectionCharacter = nextSectionStartPos - 1;

        // the first character of the section is at
        //   curPos + length(sectionStart) + 2
        // with the "+2" representing the \r\n at the
        // end of the section delimiter

        size_t firstSectionCharacter = curSectionStartPos + sectionDelimiter.size() + 2;

        // calculate the number of bytes in the section
        size_t secLen = lastSectionCharacter - firstSectionCharacter + 1;

        // extract the section data
        string data = body.substr(firstSectionCharacter, secLen);

        // append a new message part
        parts.push_back(MessagePart{data});

        // continue with the next section
        curSectionStartPos = nextSectionStartPos;
      }
    }

    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    ContentTypeHeader::ContentTypeHeader(const string& hdrBody)
      :body{hdrBody}, type{ContentType::Unknown}
    {
      // parse the content type
      StringList parts;
      stringSplitter(parts, body.getValue(), "/");
      if (parts.size() != 2)
      {
        throw MalformedHeader();
      }
      string mainType = boost::to_lower_copy(parts[0]);
      string subType = boost::to_lower_copy(parts[1]);
      boost::trim(mainType);
      boost::trim(subType);
      _isMultipart = (mainType == "multipart");
      if ((mainType == "text") && (subType == "html")) type = ContentType::Text_Html;
      if ((mainType == "text") && (subType == "plain")) type = ContentType::Text_Plain;
      if ((mainType == "multipart") && (subType == "form-data")) type = ContentType::Multipart_FormData;
    }

    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    StructuredHeaderBody::StructuredHeaderBody(const string& hdrBody)
    {
      // the header's value is everything up to the first ';'
      //
      // npos as result is fine, that means we have no ';' and thus
      // no parameters
      size_t stopPos = hdrBody.find(';');

      // extract the actual header value
      val = hdrBody.substr(0, stopPos);
      boost::trim(val);

      // find all name=value parameter pairs
      regex reParam_QuotedString{"(\\w+)\\s*=\\s*\"(.*)\""};
      regex reParam_Token{"(\\w+)\\s*=\\s*([^;=\"]+)"};
      for (const regex& re : {reParam_QuotedString, reParam_Token})
      {
        for (sregex_iterator p{hdrBody.begin()+stopPos, hdrBody.end(), re};
             p != sregex_iterator{}; ++p)
        {
          string key{(*p)[1]};
          boost::to_lower(key);   // parameter names are case insensitive
          params.emplace(key, (*p)[2]);
        }
      }
    }

    //----------------------------------------------------------------------------

    bool StructuredHeaderBody::hasParameter(const string& paraName) const
    {
      string key{boost::to_lower_copy(paraName)};

      auto it = params.find(key);
      return (it != params.end());
    }

    //----------------------------------------------------------------------------

    string StructuredHeaderBody::getParameter(const string& paraName) const
    {
      string key{boost::to_lower_copy(paraName)};

      auto it = params.find(key);
      if (it == params.end()) return "";

      return it->second;
    }

  }
}
