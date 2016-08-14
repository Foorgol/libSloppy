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
        body = rawMessage.substr(bodyStart, rawMessage.length() - bodyStart);
      }

      // extract the raw header data
      string headerString = rawMessage.substr(0, delimPos);

      // cross-check: headerString + delimiter + body must be the original message
      if ((headerString.length() + 4 + body.length()) != rawMessage.length())
      {
        throw MalformedMessage();
      }

      // parse the header
      hdr = make_unique<Header>(headerString);
    }

    //----------------------------------------------------------------------------

    string Message::getBodyData() const
    {
      return body;
    }

    //----------------------------------------------------------------------------

    Header*Message::getHeaderPtr() const
    {
      return hdr.get();
    }

    //----------------------------------------------------------------------------

  }
}
