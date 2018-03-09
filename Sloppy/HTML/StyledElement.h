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

    /** \brief A class that represents a HTML-tag like `<tag>content</tag>` with
     * support functions for easily assigning attributes, styles and CSS-clases
     */
    class StyledElement
    {
    public:
      static constexpr int MarginIgnore = -1;   ///< special value indicating that a margin value should not be set

      /** Constructs a new element from a given name
       */
      StyledElement(
          const string& _elemName,    ///< the element name without brackets, e.g., `div`
          bool _omitClosingTag=false  ///< if set to `true`, no closing tag will be generated (e.g., for `<br>`)
          );

      /** Destructor; deletes all owned elements
       */
      virtual ~StyledElement();

      /** Assigns a `name = value` pair to the element's `style` attribute
       *
       * There's no protection against assigning a value to a
       * style variable more than once.
       */
      void addStyle(
          const string& sName,   ///< the style variable to set
          const string& sValue   ///< the value to assign to this variable
          );

      /** Add a CSS-class to this element.
       *
       * There's no protection against assigning a class to
       * this element more than once.
       */
      void addClass(
          const string& cName   ///< the name of the CSS-class to add
          );

      /** Adds a `attr="value"` pair to the element.
       *
       * Internally, attribute values are stored in a map. Thus,
       * assigning an attribute more than once will overwrite the
       * previous assignments.
       */
      void addAttr(
          const string& aName,   ///< the attribute's name
          const string& aValue   ///< the attribute's value without surrounding quotes
          );

      /** Adds a child to the element
       *
       * \throws std::invalid_argument if the passed pointer is `nullptr`
       *
       * \note We take ownership of the pointer passed to this function!
       */
      void addChildElement(
          StyledElement* other  ///< pointer to another StyledElement (or derived class) that will become a child of this element
          );

      /** \returns a string that contains the HTML code for the element
       * and all its child elements.
       */
      string to_html();

      /** \brief A template function that heap-constructs a new HTML element of
       * a given type and passes the given parameters to its constructor.
       *
       * We take ownership of the newly created element. The returned
       * pointer shall only be used for accessing element functions such
       * as setting attributes, etc.
       *
       * The element type passed to this template function has to be
       * derived from (or be identical to) StyledElement.
       *
       * \returns `nullptr` if the element creation failed; a pointer to the new element otherwise
       */
      template<typename ElemType,   ///< the type of the child element to construct
               typename... Args>
      ElemType* createCustomChild(
          Args&&... args   ///< the arguments to pass to the element's constructor
          )
      {
        ElemType* newElem = new ElemType(forward<Args>(args)...);
        if (newElem == nullptr) return nullptr;

        content.push_back(newElem);
        return newElem;
      }

      /** Heap-constructs a new StyledElement instance as a child of this element
       *
       * \returns `nullptr` if the element creation failed; a pointer to the new element otherwise
       */
      StyledElement* createContentChild(
          const string& _elemName,    ///< the element name without brackets, e.g., `div`
          bool _omitClosingTag=false  ///< if set to `true`, no closing tag will be generated (e.g., for `<br>`)
          );

      /** Appends a plain text section in the element's body.
       *
       * Each element can contain multiple plain text sections that can be mixed
       * child elements.
       */
      void addPlainText(
          const string& txt   ///< the plain text to add
          );

      /** Convenience function for setting the `margin-XXX' styles of the element
       *
       * if set to the special value `MarginIgnore`, we'll create no style element for that
       * particular side.
       */
      void setMargins(
          int top,    ///< the value of the `margin-top` style
          int bottom = MarginIgnore,    ///< the value of the `margin-bottom` style
          int left = MarginIgnore,    ///< the value of the `margin-left` style
          int right = MarginIgnore    ///< the value of the `margin-right` style
          );

      /** Convenience function for setting the `text-align` style
       */
      void setTextAlignment(
          Alignment horAlign   ///< the text alignment to set for this element
          );


    protected:
      string elemName;   ///< the element's name
      string assignedClasses;   ///< a space separated list of assigned CSS-classes
      string styles;   ///< a semicolon-separated list of assigned styles
      vector<StyledElement*> content;   ///< list of pointers of child elements (OWNING pointers!)
      unordered_map<string, string> attr;   ///< a map of attribute-value pairs
      vector<string> plainTextSections;   ///< a list of all plain text content sections
      bool omitClosingTag;   ///< indicates whether we should omit the closing </tag> or not
    };
  }
}

#endif
