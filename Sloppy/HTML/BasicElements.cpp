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


  }
}
