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

#include "Bootstrap.h"

using namespace std;

namespace Sloppy
{
  namespace HTML
  {
    namespace Bootstrap
    {

      TextButton::TextButton(const string& label, const string& url, const string& additionalClass)
        :Anchor(url, label)
      {
        addClass("btn");
        if (!(additionalClass.empty())) addClass(additionalClass);
      }

      //----------------------------------------------------------------------------

      Callout::Callout(CalloutType t, const string& headline, const string& txt)
        :StyledElement("div")
      {
        addClass("callout");
        switch (t)
        {
        case CalloutType::Info:
          addClass("callout-info");
          break;

        case CalloutType::Warning:
          addClass("callout-warning");
          break;

        case CalloutType::Danger:
          addClass("callout-danger");
          break;
        }

        createCustomChild<Head>(4, headline);

        if (!(txt.empty()))
        {
          createCustomChild<Para>(txt);
        }
      }

    }
  }
}
