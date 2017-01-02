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

#ifndef SLOPPY__CONFIG_FILE_PARSER_H
#define SLOPPY__CONFIG_FILE_PARSER_H

#include <memory>
#include <istream>
#include <vector>
#include <unordered_map>

#include "../libSloppy.h"

using namespace std;


namespace Sloppy
{
  namespace ConfigFileParser
  {
    static constexpr const char* defaultSectionName = "__DEFAULT__";

    using SectionData = unordered_map<string, string>;

    class Parser
    {
    public:
      static unique_ptr<Parser> read(istream& inStream);
      static unique_ptr<Parser> read(const string& s);
      static unique_ptr<Parser> readFromFile(const string& fName);

      bool hasSection(const string& secName) const;
      bool hasKey(const string& secName, const string& keyName) const;
      bool hasKey(const string& keyName) const;

      string getValue(const string& secName, const string& keyName, bool* isOk=nullptr) const;
      string getValue(const string& keyName, bool* isOk=nullptr) const;
      bool getValueAsBool(const string& secName, const string& keyName, bool* isOk=nullptr) const;
      bool getValueAsBool(const string& keyName, bool* isOk=nullptr) const;
      int getValueAsInt(const string& secName, const string& keyName, int defaultVal = -1, bool* isOk=nullptr) const;
      int getValueAsInt(const string& keyName, int defaultVal = -1, bool* isOk=nullptr) const;

    protected:
      Parser();
      unordered_map<string, SectionData> content;
      SectionData& getOrCreateSection(const string& secName);
      void insertOrOverwriteValue(const string& secName, const string& keyName, const string& val);
      void insertOrOverwriteValue(SectionData& sec, const string& keyName, const string& val);
    };


  }
}
#endif
