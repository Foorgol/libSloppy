/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2017  Volker Knollmann
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

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

      explicit StyledElement(const string& _elemName, bool _omitClosingTag=false);
      virtual ~StyledElement();

      void addStyle(const string& sName, const string& sValue);
      void addClass(const string& cName);
      void addAttr(const string& aName, const string& aValue);
      void addContentElement(StyledElement* other); // TAKES OWNERSHIP!!
      string to_html();

      template<typename ElemType, typename... Args>
      ElemType* createCustomContentChild(Args&&... args)
      {
        ElemType* newElem = new ElemType(forward<Args>(args)...);
        if (newElem == nullptr) return nullptr;

        content.push_back(newElem);
        return newElem;
      }

      StyledElement* createContentChild(const string& _elemName, bool _omitClosingTag=false);

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
      bool omitClosingTag;
    };
  }
}

#endif
