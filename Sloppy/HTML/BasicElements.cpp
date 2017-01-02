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

    Input::Input(const string& id, InputType it, const string& value, const string& content, const string& name)
      :StyledElement("input", true)
    {
      addAttr("id", id);
      addAttr("name", name.empty() ? id : name);

      string sType;
      switch (it)
      {
      case InputType::CheckBox:
        sType = "checkbox";
        break;

      case InputType::Radio:
        sType = "radio";
        break;

      case InputType::Text:
        sType = "text";
        break;

      case InputType::Hidden:
        sType = "hidden";
        break;

      default:
        sType = "text";
      }
      addAttr("type", sType);

      if (!(value.empty()))
      {
        addAttr("value", value);
      }

      if (!(content.empty()))
      {
        setPlainTextContent(content);
      }
    }


    //----------------------------------------------------------------------------


  }
}
