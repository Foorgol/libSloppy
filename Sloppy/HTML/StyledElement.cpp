#include "StyledElement.h"

namespace Sloppy
{
  namespace HTML
  {

    StyledElement::StyledElement(const string& _elemName)
      :elemName{_elemName}
    {

    }

    //----------------------------------------------------------------------------

    void StyledElement::addStyle(const string& sName, const string& sValue)
    {
      if (!(styles.empty())) styles += " ";
      styles += sName + ": " + sValue + ";";
    }

    //----------------------------------------------------------------------------

    void StyledElement::addClass(const string& cName)
    {
      if (!(assignedClasses.empty())) assignedClasses += " ";
      assignedClasses += cName;
    }

    //----------------------------------------------------------------------------

    void StyledElement::addAttr(const string& aName, const string& aValue)
    {
      attr[aName] = aValue;
    }

    //----------------------------------------------------------------------------

    void StyledElement::addContentElement(StyledElement* other)
    {
      content.push_back(other);
    }

    //----------------------------------------------------------------------------

    string StyledElement::to_html()
    {
      string result = "<" + elemName;

      // add classes and styles
      if (!(assignedClasses.empty())) attr["class"] = assignedClasses;
      if (!(styles.empty())) attr["style"] = styles;

      // add all HTML attributes
      for (auto& a : attr)
      {
        result += " ";
        result += a.first;
        result += "=\"" + a.second + "\"";
      }

      // close the opening tag
      result += ">";

      // render content
      //
      // if plainTextContent is set, we take that. It overrides everything
      // else. If not, we iterate over possibly existing child elements
      if (plainTextContent.empty())
      {
        for (StyledElement* e : content)
        {
          if (e != nullptr) result += e->to_html();
        }
      } else {
        result += plainTextContent;
      }

      // write the closing tag
      result += "</" + elemName + ">";

      // done
      return result;
    }

    //----------------------------------------------------------------------------

    StyledElement* StyledElement::createContentChild(const string& _elemName)
    {
      StyledElement* newElem = new StyledElement(_elemName);
      if (newElem == nullptr) return nullptr;

      content.push_back(newElem);
      return newElem;
    }

    //----------------------------------------------------------------------------

    StyledElement::~StyledElement()
    {
      for (StyledElement* e : content)
      {
        delete e;
      }
    }


    //----------------------------------------------------------------------------


  }
}
