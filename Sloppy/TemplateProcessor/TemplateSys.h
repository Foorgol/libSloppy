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

#ifndef SLOPPY__TEMPLATE_SYSTEM_H
#define SLOPPY__TEMPLATE_SYSTEM_H

#include <istream>
#include <unordered_map>
#include <memory>
#include <vector>

#include "../libSloppy.h"

using namespace std;

namespace Sloppy
{
  namespace TemplateSystem
  {
    class RawTemplate
    {
    public:
      explicit RawTemplate(istream& inData);
      explicit RawTemplate(const string& inData);
      static unique_ptr<RawTemplate> fromFile(const string& fName);

    protected:
      string data;
    };

    //----------------------------------------------------------------------------

    class TemplateStore
    {
    public:
      explicit TemplateStore(const string& rootDir, const StringList& extList);

    private:
      unordered_map<string, RawTemplate> rawData;
    };
  }
}

#endif
