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

#ifndef SLOPPY__TEMPLATE_H
#define SLOPPY__TEMPLATE_H

#include <istream>
#include <unordered_map>
#include <memory>
#include <vector>

using namespace std;

namespace Sloppy
{
  namespace TemplateProcessor
  {
    using SubstDic = unordered_map<string, string>;
    using SubstDicList = vector<SubstDic>;

    class Template
    {
    public:
      explicit Template(istream& inData);
      explicit Template(const string& inData);
      static unique_ptr<Template> fromFile(const string& fName);
      void getSubstitutedData(string& outString, const SubstDic& dic=SubstDic{}, const string& keyPrefix="", const string& keyPostfix="") const;
      void applyPermanentSubstitution(const SubstDic& dic, const string& keyPrefix="", const string& keyPostfix="");
      void doForEachLoop(string& outString, SubstDicList dl, const string& keyPrefix="", const string& keyPostfix="", const string& delim="") const;

    protected:
      string data;
    };
  }
}

#endif
