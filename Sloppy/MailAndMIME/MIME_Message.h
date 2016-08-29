#ifndef SLOPPY__MAIL_AND_MIME__MIME_MESSAGE_H
#define SLOPPY__MAIL_AND_MIME__MIME_MESSAGE_H

#include <string>
#include <memory>
#include <unordered_map>

#include "MailAndMIME.h"
#include "Header.h"
#include "Message.h"

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


    enum class ContentType
    {
      Unknown,
      Text_Html,
      Text_Plain,
      Multipart_FormData,
      // will grow over time
    };

    //----------------------------------------------------------------------------

    class ContentTypeHeader
    {
    public:
      explicit ContentTypeHeader()
        :type{ContentType::Unknown} {}

      explicit ContentTypeHeader(const string& hdrBody);
      ContentType getType() const;
      bool hasParam(const string& pName) const;
      string getParam(const  string& pName) const;
      bool isMultipart() const;

    protected:
      string body;
      ContentType type;
      unordered_map<string, string> params;
      bool _isMultipart;
    };

    //----------------------------------------------------------------------------
    class MessagePart
    {
    public:
      MessagePart(const string& _content)
        :content{_content} {}

      inline bool hasSubParts() { return subParts.size() > 0; }

      inline string getContent() const { return content; }

    protected:
      string content;
      vector<MessagePart> subParts;
    };

    //----------------------------------------------------------------------------

    class MIME_Message
    {
    public:
      explicit MIME_Message(const RFC822::Message& inMsg, bool skipMIMEVersionCheck=true);
      inline int getNumParts() const { return parts.size(); }
      string getPart(int i) const;
      inline ContentType getContentType() { return ctHeader.getType(); }

    protected:
      void parseParts(const string& body);

    private:
      string body;
      ContentTypeHeader ctHeader;
      vector<MessagePart> parts;
    };

    //----------------------------------------------------------------------------


  }
}

#endif
