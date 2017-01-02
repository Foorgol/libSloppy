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

#ifndef SLOPPY__HTML__BOOTSTRAP_H
#define SLOPPY__HTML__BOOTSTRAP_H

#include <string>
#include <vector>
#include <unordered_map>

#include "StyledElement.h"
#include "BasicElements.h"

using namespace std;

namespace Sloppy
{
  namespace HTML
  {
    namespace Bootstrap
    {
      class TextButton : public Anchor
      {
      public:
        explicit TextButton(const string& label, const string& url, const string& additionalClass = "btn-default");
        virtual ~TextButton(){}
      };

      //----------------------------------------------------------------------------

      enum class GlyphiconName
      {
        Trash,
        User,
        Okay,
        Home,
        Refresh,
        Alert,
        Wrench,
        Link,
        Off,
        SignQuestion,
        SignInformation,
        SignExclamation,
        Pencil,
        Plus
      };

      string to_string(GlyphiconName gn);

      //----------------------------------------------------------------------------

      class Glyphicon : public Span
      {
      public:
        explicit Glyphicon(const string& plainGlyName);
        explicit Glyphicon(GlyphiconName gn)
          :Glyphicon(to_string(gn)) {}

        virtual ~Glyphicon(){}
      };

      //----------------------------------------------------------------------------

      class GlyphiconButton : public TextButton
      {
      public:
        explicit GlyphiconButton(const string& plainGlyName, const string& url, const string& additionalClass = "btn-default");
        explicit GlyphiconButton(GlyphiconName gn, const string& url, const string& additionalClass = "btn-default")
          :GlyphiconButton(to_string(gn), url, additionalClass) {}

        virtual ~GlyphiconButton(){}
      };

      //----------------------------------------------------------------------------

      enum class CalloutType
      {
        Info,
        Warning,
        Danger
      };

      class Callout : public StyledElement
      {
      public:
        Callout(CalloutType t, const string& headline, const string& txt="");
        virtual ~Callout() {}
      };

      //----------------------------------------------------------------------------

    }
  }
}

#endif
