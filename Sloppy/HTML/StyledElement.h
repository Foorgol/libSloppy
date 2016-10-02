#ifndef SLOPPY__HTML__STYLED_ELEMENT_H
#define SLOPPY__HTML__STYLED_ELEMENT_H

#include <string>
#include <vector>
#include <unordered_map>

using namespace std;

namespace Sloppy
{
  namespace HTML
  {
    enum class Alignment
    {
      Left,
      Center,
      Right,
      Default
    };

    //----------------------------------------------------------------------------

    class StyledElement
    {
    public:
      static constexpr int MarginIgnore = -1;

      explicit StyledElement(const string& _elemName);
      virtual ~StyledElement();

      void addStyle(const string& sName, const string& sValue);
      void addClass(const string& cName);
      void addAttr(const string& aName, const string& aValue);
      void addContentElement(StyledElement* other); // TAKES OWNERSHIP!!
      string to_html();

      template<typename ElemType>
      ElemType* createCustomContentChild()
      {
        ElemType* newElem = new ElemType;
        if (newElem == nullptr) return nullptr;

        content.push_back(newElem);
        return newElem;
      }

      StyledElement* createContentChild(const string& _elemName);

      void setPlainTextContent(const string& ptc) { plainTextContent = ptc; }

      void setMargins(int top, int bottom = MarginIgnore, int left = MarginIgnore, int right = MarginIgnore);
      void setTextAlignment(Alignment horAlign);


    protected:
      string elemName;
      string assignedClasses;
      string styles;
      vector<StyledElement*> content;
      unordered_map<string, string> attr;
      string plainTextContent;
    };
  }
}

#endif
