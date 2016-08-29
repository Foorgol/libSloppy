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

    class StructuredHeaderBody
    {
    public:
      explicit StructuredHeaderBody(const string& hdrBody);
      bool hasParameter(const string& paraName) const;
      string getParameter(const string& paraName) const;
      inline string getValue() const { return val; }

    protected:
      string val;
      unordered_map<string, string> params;
    };

    //----------------------------------------------------------------------------

    class ContentTypeHeader
    {
    public:
      explicit ContentTypeHeader()
        :body{""}, type{ContentType::Unknown}, _isMultipart{false} {}

      explicit ContentTypeHeader(const string& hdrBody);
      inline ContentType getType() const { return type; }
      bool hasParam(const string& pName) const { return body.hasParameter(pName); }
      string getParam(const  string& pName) const { return body.getParameter(pName); }
      inline bool isMultipart() const { return _isMultipart; }

    protected:
      StructuredHeaderBody body;
      ContentType type;
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
