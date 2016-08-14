#ifndef SLOPPY__MAIL_AND_MIME__MESSAGE_H
#define SLOPPY__MAIL_AND_MIME__MESSAGE_H

#include <string>
#include <memory>

#include "MailAndMIME.h"
#include "Header.h"

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

    // concrete class
    class Message
    {
    public:
      explicit Message(const string& rawMessage);

      string getBodyData() const;

      Header* getHeaderPtr() const;

    private:
      unique_ptr<Header> hdr;
      string body;
    };

  }
}

#endif
