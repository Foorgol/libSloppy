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

#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

#include "Template.h"
#include "../libSloppy.h"

namespace Sloppy
{
  namespace TemplateProcessor
  {

    Template::Template(istream& inData)
      // copy the complete stream until EOF into
      // an internal buffer
      :data{std::istreambuf_iterator<char>{inData}, {}}
    {
    }

    //----------------------------------------------------------------------------

    Template::Template(const string& inData)
      :data{inData}
    {
    }

    //----------------------------------------------------------------------------

    unique_ptr<Template> Template::fromFile(const string& fName)
    {
      ifstream f{fName, ios::binary};
      if (!f) return nullptr;

      return make_unique<Template>(f);
    }

    //----------------------------------------------------------------------------

    void Template::getSubstitutedData(string& outString, const SubstDic& dic, const string& keyPrefix, const string& keyPostfix) const
    {
      // copy the unsubstituted data to the result variable
      outString = data;

      // sort keys by length and use long keys first,
      // otherwise, a long key would be corrupted if it
      // contains a shorter key and that shorter key is
      // applied first
      StringList keyList;
      unordered_map<string, string> full2short;  // don't use a "full --> data" here but "full --> short --> data" to avoid copying the possible large data part
      for (const auto& s : dic)
      {
        string fullKey = keyPrefix + s.first + keyPostfix;
        keyList.push_back(fullKey);
        full2short.emplace(fullKey, s.first);
      }
      std::sort(keyList.begin(), keyList.end(), [](const string& s1, const string& s2) {
        return (s1.length() > s2.length());
      });

      // apply all substitutions
      for (const string& fullKey : keyList)
      {
        const string& srcKey = full2short[fullKey];
        auto it = dic.find(srcKey);
        replaceString_All(outString, fullKey, it->second);
      }
    }

    //----------------------------------------------------------------------------

    void Template::applyPermanentSubstitution(const SubstDic& dic, const string& keyPrefix, const string& keyPostfix)
    {
      string newData;
      getSubstitutedData(newData, dic, keyPrefix, keyPostfix);
      data = newData;
    }

    //----------------------------------------------------------------------------

    void Template::doForEachLoop(string& outString, SubstDicList dl, const string& keyPrefix, const string& keyPostfix, const string& delim) const
    {
      outString.clear();
      for (auto it = dl.begin(); it != dl.end(); ++it)
      {
        if ((!(delim.empty())) && (it != dl.begin()))
        {
          outString.append(delim);
        }

        string thisSubst;
        getSubstitutedData(thisSubst, *it, keyPrefix, keyPostfix);
        outString.append(thisSubst);
      }
    }

    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------

  }
}
