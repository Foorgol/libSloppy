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

#ifndef SLOPPY__HTML__BASIC_ELEMENTS_H
#define SLOPPY__HTML__BASIC_ELEMENTS_H

#include <string>
#include <vector>
#include <unordered_map>

#include "StyledElement.h"

using namespace std;

namespace Sloppy
{
  namespace HTML
  {
    /** \brief A helper class for easily constructing simple HTML tags such as `&lt;p&gt;`, `&lt;span&gt;` and the like.
     *
     * All these elements have in common that they (mostly)
     * consist of a plain text context and that they can have
     * a horizontal alignment.
     */
    class ElementWithTextAndHorAlignment : public StyledElement
    {
    public:
      /** Ctor
       */
      ElementWithTextAndHorAlignment(
          const string& elName,   ///< the name of the element to construct
          const string& content,   ///< the content within the `<xxx>' tags
          Alignment horAlignment = Alignment::Default   ///< the horizontal alignment of the content text
          );
    };

    /** \brief A `&lt;span&gt;' element
     */
    class Span : public ElementWithTextAndHorAlignment
    {
    public:
      /** Ctor for a `&lt;span&gt;' element with plain text content
       */
      explicit Span(
          const string& content,   ///< the content within the `<span>` tags
          Alignment horAlignment = Alignment::Default   ///< the horizontal alignment of the content text
          )
        : ElementWithTextAndHorAlignment("span", content, horAlignment) {}

      virtual ~Span(){}
    };

    //----------------------------------------------------------------------------

    /** \brief A `&lt;p&gt;' element
     */
    class Para : public ElementWithTextAndHorAlignment
    {
    public:
      /** Ctor for a `&lt;p&gt;' element with plain text content
       */
      explicit Para(
          const string& content,   ///< the content within the `<p>` tags
          Alignment horAlignment = Alignment::Default   ///< the horizontal alignment of the content text
          )
        :ElementWithTextAndHorAlignment("p", content, horAlignment) {}

      virtual ~Para(){}
    };

    //----------------------------------------------------------------------------

    /** \brief A `&lt;h1&gt;', `&lt;h2&gt;', `&lt;h3&gt;'... header element
     */
    class Head : public ElementWithTextAndHorAlignment
    {
    public:
      explicit Head(
          int lvl,   ///< the headline level, starting at 1. No range checks applied!
          const string& content,   ///< the header itself
          Alignment horAlignment = Alignment::Default   ///< the horizontal alignment of the header
          )
        :ElementWithTextAndHorAlignment("h" + to_string(lvl), content, horAlignment) {}

      virtual ~Head(){}
    };

    //----------------------------------------------------------------------------

    /** \brief A `<a href="xxx">SomeText</a>` element
     */
    class Anchor : public StyledElement
    {
    public:
      explicit Anchor(
          const string& url,   ///< the value for the `href` attribute
          const string& linkText=""   ///< the inner text
          );

      virtual ~Anchor(){}
    };

    //----------------------------------------------------------------------------

    /** Request method for submitting form data
     */
    enum class FormMethod
    {
      Post,   ///< use a POST request
      Get     ///< use a GET request
    };

    /** \brief A '<form>' element
     */
    class Form : public StyledElement
    {
    public:
      /** Ctor
       */
      Form(
          const string& id,   ///< the value to assign to the form's `id` attribute
          const string& targetUrl,   ///< the submission URL for the form data (==> value of the `action` attribute)
          FormMethod method = FormMethod::Post,    ///< whether to use a GET (url-encoded) or POST (multipart) request for submission
          const string& encType = "multipart/form-data"   ///< the value of the `enctype` attribute
          );

      virtual ~Form(){}
    };

    //----------------------------------------------------------------------------

    /** enum of supported types of `<input>` elements
     */
    enum class InputType
    {
      Text,   ///< text input (`type="text"`)
      Radio,  ///< radio button (`type="radio"`)
      CheckBox, ///< checkbox (`type="checkbox"`)
      Hidden   ///< invisible input (`type="hidden"`)
    };

    /** \brief A `<input>` element
     */
    class Input : public StyledElement
    {
    public:
      Input(
          const string& id,   ///< the value to assign to the input's `id` attribute
          InputType it,   ///< the type of element to create
          const string& value="",   ///< the value to assign to the input's `value` attribute
          const string& content="",   ///< the content between the opening and closing tags
          const string& name=""   ///< the value for the `name` attribute; defaults to the `id` if empty
          );

      virtual ~Input(){}
    };
  }
}

#endif
