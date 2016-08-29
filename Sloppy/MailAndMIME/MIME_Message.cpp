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
      return (i >= parts.size()) ? string{} : parts[i].getContent();
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
    {
      // find the first semicolon
      //
      // npos as result is fine
      size_t stopPos = hdrBody.find(';');

      // extract the actual content type
      string typeStr = hdrBody.substr(0, stopPos);

      // parse the string
      StringList parts;
      stringSplitter(parts, typeStr, "/");
      if (parts.size() != 2)
      {
        throw MalformedHeader();
      }
      type = ContentType::Unknown;
      string mainType = boost::to_lower_copy(parts[0]);
      string subType = boost::to_lower_copy(parts[1]);
      boost::trim(mainType);
      boost::trim(subType);
      _isMultipart = (mainType == "multipart");
      if ((mainType == "text") && (subType == "html")) type = ContentType::Text_Html;
      if ((mainType == "text") && (subType == "plain")) type = ContentType::Text_Plain;
      if ((mainType == "multipart") && (subType == "form-data")) type = ContentType::Multipart_FormData;

      // extract potential parameters
      StringList paramList;
      stringSplitter(paramList, hdrBody, ";");
      if (paramList.size() > 1)
      {
        auto it = paramList.begin();
        ++it;   // skip the first "parameter", because that's the content type

        while (it != paramList.end())
        {
          StringList kv;
          stringSplitter(kv, *it, "=");

          string key = kv[0];
          boost::to_lower(key);
          boost::trim(key);

          string value;
          if (kv.size() > 1) value = kv[1];
          boost::trim(value);

          params.emplace(key, value);

          ++it;
        }
      }
    }

    //----------------------------------------------------------------------------

    ContentType ContentTypeHeader::getType() const
    {
      return type;
    }

    //----------------------------------------------------------------------------

    bool ContentTypeHeader::hasParam(const string& pName) const
    {
      auto it = params.find(boost::to_lower_copy(pName));
      return (it != params.end());
    }

    //----------------------------------------------------------------------------

    string ContentTypeHeader::getParam(const string& pName) const
    {
      auto it = params.find(boost::to_lower_copy(pName));
      if (it == params.end()) return "";

      return it->second;
    }

    //----------------------------------------------------------------------------

    bool ContentTypeHeader::isMultipart() const
    {
      return _isMultipart;
    }
  }
}
