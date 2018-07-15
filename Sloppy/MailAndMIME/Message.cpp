/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2018  Volker Knollmann
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

#include "MailAndMIME.h"
#include "Message.h"

namespace Sloppy
{
  namespace RFC822
  {

    Message::Message(const string& rawMessage)
      :hdr{nullptr}, body{}
    {
      if (rawMessage.empty())
      {
        throw EmptyMessage();
      }

      // we need at least one CRLFCRLF-sequence that indicates
      // the separation between header and body
      size_t delimPos = rawMessage.find(sCRLFCRLF);
      if (delimPos == string::npos)  // no delimiter found
      {
        throw MalformedMessage();
      }
      if (delimPos == 0)  // empty header
      {
        throw MalformedMessage();
      }

      // copy the message body to our internal data structure
      size_t bodyStart = delimPos + 4;
      if (bodyStart < rawMessage.length())
      {
        body = rawMessage.substr(bodyStart);
      }

      // extract the raw header data
      string headerString = rawMessage.substr(0, delimPos);

      // cross-check: headerString + delimiter + body must be the original message
      if ((headerString.length() + 4 + body.length()) != rawMessage.length())
      {
        throw MalformedMessage();
      }

      // parse the header
      try
      {
        hdr = make_unique<Header>(headerString);
      } catch (...)
      {
        throw MalformedMessage();
      }
    }

    //----------------------------------------------------------------------------

    string Message::getBodyData() const
    {
      return body;
    }

    //----------------------------------------------------------------------------

    const Header* Message::getHeaderPtr() const
    {
      return hdr.get();
    }

    //----------------------------------------------------------------------------

  }
}
