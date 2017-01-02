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
    class Span : public StyledElement
    {
    public:
      explicit Span(const string& content, Alignment horAlignment = Alignment::Default);
      virtual ~Span(){}
    };

    //----------------------------------------------------------------------------

    class Para : public Span
    {
    public:
      explicit Para(const string& content, Alignment horAlignment = Alignment::Default)
        :Span(content, horAlignment) { elemName = "p"; }
      virtual ~Para(){}
    };

    //----------------------------------------------------------------------------

    class Head : public Span
    {
    public:
      explicit Head(int lvl, const string& content, Alignment horAlignment = Alignment::Default)
        :Span(content, horAlignment) { elemName = "h" + to_string(lvl); }
      virtual ~Head(){}
    };

    //----------------------------------------------------------------------------

    class Anchor : public StyledElement
    {
    public:
      explicit Anchor(const string& url, const string& linkText="");
      virtual ~Anchor(){}
    };

    //----------------------------------------------------------------------------

    enum class FormMethod
    {
      Post,
      Get
    };

    class Form : public StyledElement
    {
    public:
      Form(const string& id, const string& targetUrl, FormMethod method = FormMethod::Post, const string& encType = "multipart/form-data");
      virtual ~Form(){}
    };

    //----------------------------------------------------------------------------

    enum class InputType
    {
      Text,
      Radio,
      CheckBox,
      Hidden
    };

    class Input : public StyledElement
    {
    public:
      Input(const string& id, InputType it, const string& value="", const string& content="", const string& name="");
      virtual ~Input(){}
    };
  }
}

#endif
