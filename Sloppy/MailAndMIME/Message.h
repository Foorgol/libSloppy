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

#ifndef SLOPPY__MAIL_AND_MIME__MESSAGE_H
#define SLOPPY__MAIL_AND_MIME__MESSAGE_H

#include <memory>

#include "MailAndMIME.h"
#include "Header.h"
#include "../String.h"

using namespace std;

namespace Sloppy
{
  namespace RFC822
  {
    // Forward declarations
    class Header;

    // Exceptions
    struct MalformedMessage {};
    struct EmptyMessage {};

    /** \brief A class representing a RFC 822 compliant email message.
     */
    class Message
    {
    public:
      /** \brief Ctor from the raw message data, e.g. delivered via SNMP.
       *
       * \throws EmptyMessage if the message data is empty
       * \throws MalformedMessage if the message could not be parsed
       */
      explicit Message(const string& rawMessage   ///< a string containing the raw message data
          );

      /** \returns the raw body part of the message
       */
      string getBodyData() const;

      /** \returns a pointer to an object representing the email header
       *
       * The header object is owned by this class and the user may not delete it.
       */
      const Header* getHeaderPtr() const;

    private:
      unique_ptr<Header> hdr;   ///< pointer to an object with the header data
      string body;   ///< the message body
    };

  }
}

#endif
