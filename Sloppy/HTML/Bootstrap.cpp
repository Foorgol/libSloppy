#include "Bootstrap.h"

namespace Sloppy
{
  namespace HTML
  {
    namespace Bootstrap
    {

      TextButton::TextButton(const string& label, const string& url, const string& additionalClass)
        :Anchor(url, label)
      {
        addClass("btn");
        if (!(additionalClass.empty())) addClass(additionalClass);
      }

      //----------------------------------------------------------------------------

      string to_string(GlyphiconName gn)
      {
        switch (gn)
        {
        case GlyphiconName::Alert:
          return "alert";

        case GlyphiconName::Home:
          return "home";

        case GlyphiconName::Link:
          return "link";

        case GlyphiconName::Okay:
          return "ok";

        case GlyphiconName::Refresh:
          return "refresh";

        case GlyphiconName::Trash:
          return "trash";

        case GlyphiconName::User:
          return "user";

        case GlyphiconName::Wrench:
          return "wrench";

        case GlyphiconName::Off:
          return "off";

        case GlyphiconName::SignQuestion:
          return "question-sign";

        case GlyphiconName::SignExclamation:
          return "exclamation-sign";

        case GlyphiconName::SignInformation:
          return "info-sign";

        case GlyphiconName::Pencil:
          return "pencil";

        case GlyphiconName::Plus:
          return "plus";

        default:
          return "asterisk";
        }

        return "asterisk";
      }

      //----------------------------------------------------------------------------

      Glyphicon::Glyphicon(const string& plainGlyName)
        :Span("")
      {
        addClass("glyphicon");
        addClass("glyphicon-" + plainGlyName);
      }

      //----------------------------------------------------------------------------

      GlyphiconButton::GlyphiconButton(const string& plainGlyName, const string& url, const string& additionalClass)
        :TextButton("", url, additionalClass)
      {
        plainTextContent.clear();
        Glyphicon* g = new Glyphicon(plainGlyName);
        addContentElement(g);
      }

      //----------------------------------------------------------------------------

      Callout::Callout(CalloutType t, const string& headline, const string& txt)
        :StyledElement("div")
      {
        addClass("callout");
        switch (t)
        {
        case CalloutType::Info:
          addClass("callout-info");
          break;

        case CalloutType::Warning:
          addClass("callout-warning");
          break;

        case CalloutType::Danger:
          addClass("callout-danger");
          break;
        }

        auto hdr = new Head(4, headline);
        addContentElement(hdr);

        if (!(txt.empty()))
        {
          addContentElement(new Para(txt));
        }
      }


      //----------------------------------------------------------------------------

    }
  }
}
