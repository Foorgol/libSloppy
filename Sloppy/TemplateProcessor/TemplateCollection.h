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

#ifndef SLOPPY__TEMPLATE_COLLECTION_H
#define SLOPPY__TEMPLATE_COLLECTION_H

#include <istream>
#include <unordered_map>
#include <memory>

#include "Template.h"

using namespace std;

namespace Sloppy
{
  namespace TemplateProcessor
  {
    using TemplateStore = unordered_map<string, Template>;

    class TemplateCollection
    {
    public:
      TemplateCollection() {}
      TemplateCollection(const string& dataDir);

      bool addTemplate(const string& shortName, istream& inData);
      bool addTemplate(const string& shortName, const string& inData);
      bool addTemplateFromFile(const string& shortName, const string& fName);

      bool removeTemplate(const string& shortName);

      bool replaceTemplate(const string& shortName, istream& inData);
      bool replaceTemplate(const string& shortName, const string& inData);

      void applyPermanentSubstitution(const string& shortName, const SubstDic& dic, const string& keyPrefix="", const string& keyPostfix="");

      bool getSubstitutedData(const string& shortName, string& outString, const SubstDic& dic=SubstDic{}, const string& keyPrefix="", const string& keyPostfix="") const;

      bool doForEachLoop(const string& shortName, string& outString, SubstDicList dl, const string& keyPrefix="", const string& keyPostfix="", const string& delim="") const;

    protected:
      TemplateStore store;
    };
  }
}

#endif
