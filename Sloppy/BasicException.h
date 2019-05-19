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

#ifndef __LIBSLOPPY_BASICEXCEPTION_H
#define __LIBSLOPPY_BASICEXCEPTION_H

#include <string>
#include <iostream>

namespace Sloppy
{
  /** \brief A generic networking exception used for error reporting
   */
  class BasicException
  {
  public:
    BasicException(const std::string& exName, const std::string& sender, const std::string& context = std::string{}, const std::string& details = std::string{})
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

    std::string what() { return msg; }

    void say() const { std::cerr << msg << std::endl; }

  protected:
    std::string msg{};
  };
}

#endif
