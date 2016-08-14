#ifndef SLOPPY__MAIL_AND_MIME__HEADER_H
#define SLOPPY__MAIL_AND_MIME__HEADER_H

#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>

#include "../../libSloppy.h"
#include "MailAndMIME.h"

using namespace std;

namespace Sloppy
{
  namespace RFC822
  {
    // exception
    struct MalformedHeader {};

    //----------------------------------------------------------------------------

    class HeaderField
    {
    public:
      explicit HeaderField(const string& fName, const string& fBody);

      inline bool operator ==(const string& fName) const
      {
        return (fieldName == boost::to_lower_copy(fName));
      }

      inline bool operator !=(const string& fName) const
      {
        return (fieldName != boost::to_lower_copy(fName));
      }

      inline string getRawBody() const
      {
        return fieldBody_raw;
      }

      inline string getBody() const
      {
        return fieldBody;
      }

    protected:
      string fieldName;
      string fieldBody_raw;
      string fieldBody;

    private:
      string removeCommentsFromBody(const string& rawBody);
    };

    //----------------------------------------------------------------------------

    class Header
    {
    public:
      explicit Header() {}
      explicit Header(const string& rawHeaderData);

      StringList getRawFieldBody(const string& fieldName) const;
      bool hasField(const string& fieldName) const;
      inline int getFieldCount() const
      {
        return fields.size();
      }
      string getRawFieldBody_Simple(const string& fieldName) const;
      string getFieldBody_Simple(const string& fieldName) const;

    private:
      vector<HeaderField> fields;
    };

  }
}
#endif
