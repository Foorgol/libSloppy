/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2018  Volker Knollmann
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

#ifndef __LIBSLOPPY_BASICEXCEPTION_H
#define __LIBSLOPPY_BASICEXCEPTION_H

#include <string>
#include <iostream>

using namespace std;

namespace Sloppy
{
  /** \brief A generic networking exception used for error reporting
   */
  class BasicException
  {
  public:
    BasicException(const string& exName, const string& sender, const string& context = string{}, const string& details = string{})
    {
      msg = "Exception thrown! Here's what happened:\n";
      msg += "    Type: " + exName + "\n";
      msg += "    Sender: " + sender + "\n";
      if (!(context.empty()))
      {
        msg += "    Context: " + context + "\n";
      }
      if (!(details.empty()))
      {
        msg += "    Details: " + details + "\n";
      }

      say();
    }

    string what() { return msg; }

    void say() const { cerr << msg << endl; }

  protected:
    string msg{};
  };
}

#endif
