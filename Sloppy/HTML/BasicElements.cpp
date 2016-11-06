#include "BasicElements.h"

namespace Sloppy
{
  namespace HTML
  {

    Span::Span(const string& content, Alignment horAlignment)
      :StyledElement("span")
    {
      setTextAlignment(horAlignment);
      setPlainTextContent(content);
    }

    //----------------------------------------------------------------------------

    Anchor::Anchor(const string& url, const string& linkText)
      :StyledElement("a")
    {
      addAttr("href", url);
      setPlainTextContent(linkText);
    }

    //----------------------------------------------------------------------------

    Form::Form(const string& id, const string& targetUrl, FormMethod method, const string& encType)
      :StyledElement("form")
    {
      addAttr("id", id);
      addAttr("action", targetUrl);
      addAttr("method", (method == FormMethod::Get) ? "get" : "post");
      addAttr("enctype", encType);
    }


    //----------------------------------------------------------------------------


  }
}
