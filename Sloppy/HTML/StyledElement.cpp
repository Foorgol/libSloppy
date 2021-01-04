/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2021  Volker Knollmann
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

#include <stdexcept>

#include "StyledElement.h"

using namespace std;

namespace Sloppy
{
  namespace HTML
  {

    StyledElement::StyledElement(const string& _elemName, bool _omitClosingTag)
      :elemName{_elemName}, omitClosingTag{_omitClosingTag}
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

    void StyledElement::addChildElement(StyledElement* other)
    {
      if (other == nullptr)
      {
        throw std::invalid_argument("StyledElement: received nullptr for adding a child element");
      }

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

      //
      // render content
      //

      // index of the next plain text string in the list
      int idxNextText = 0;

      // iteratively add the HTML-representation of each child
      // or of the next plain text section
      for (StyledElement* e : content)
      {
        if (e != nullptr)
        {
          result += e->to_html();
        } else {
          result += plainTextSections.at(idxNextText);
          ++idxNextText;
        }
      }

      // write the closing tag, if necessary
      if (!omitClosingTag) result += "</" + elemName + ">";

      // done
      return result;
    }

    //----------------------------------------------------------------------------

    StyledElement* StyledElement::createContentChild(const string& _elemName, bool _omitClosingTag)
    {
      StyledElement* newElem = new StyledElement(_elemName, _omitClosingTag);
      if (newElem == nullptr) return nullptr;

      content.push_back(newElem);
      return newElem;
    }

    //----------------------------------------------------------------------------

    void StyledElement::addPlainText(const string& txt)
    {
      plainTextSections.push_back(txt);

      // store a nullptr in the content list to indicate
      // that a plain text section follows
      content.push_back(nullptr);
    }

    //----------------------------------------------------------------------------

    void StyledElement::setMargins(int top, int bottom, int left, int right)
    {
      for (pair<string, int>& p : vector<pair<string, int>>{{"top", top}, {"bottom", bottom}, {"left", left}, {"right", right}})
      {
        if (p.second != MarginIgnore)
        {
          addStyle("margin-" + p.first, to_string(p.second) + "px");
        }
      }
    }

    //----------------------------------------------------------------------------

    void StyledElement::setTextAlignment(Alignment horAlign)
    {
      if (horAlign != Alignment::Default)
      {
        string a;
        switch (horAlign)
        {
        case Alignment::Left:
          a = "left";
          break;

        case Alignment::Center:
          a = "center";
          break;

        case Alignment::Right:
          a = "right";
          break;
        }

        addStyle("text-align", a);
      }
    }

    //----------------------------------------------------------------------------

    void StyledElement::deleteAllContent()
    {
      for (StyledElement* e : content)
      {
        if (e != nullptr) delete e;
      }

      content.clear();
      plainTextSections.clear();
    }

    //----------------------------------------------------------------------------

    StyledElement::~StyledElement()
    {
      deleteAllContent();
    }


    //----------------------------------------------------------------------------


  }
}
