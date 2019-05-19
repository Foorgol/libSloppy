/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2019  Volker Knollmann
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
      /** \brief A text button with a `onclick`-URL
       *
       * Basically creates an anchor element `&lt;a&gt;` with the `btn` class and
       * optional additional CSS classes assigned.
       */
      class TextButton : public Anchor
      {
      public:
        /** \brief Ctor for the text button
         */
        TextButton(
            const string& label,   ///< the button's label
            const string& url,     ///< the URL to call on click
            const string& additionalClass = "btn-default"   ///< any additional CSS classes to assign to the button
            );

        virtual ~TextButton(){}
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
