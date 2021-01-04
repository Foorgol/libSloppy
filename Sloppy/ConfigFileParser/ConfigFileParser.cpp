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

#include <string>
#include <regex>
#include <iostream>
#include <sstream>
#include <fstream>

#include "ConfigFileParser.h"
#include "../String.h"
#include "../DateTime/DateAndTime.h"

using namespace std;

namespace Sloppy
{
  Parser::Parser(istream& inStream)
  {
    if (!inStream)
    {
      throw std::invalid_argument("Config parser received invalid / empty data stream");
    }

    fillFromStream(inStream);
  }

  //----------------------------------------------------------------------------

  Parser::Parser(const string& fName)
  {
    ifstream inStream{fName, ios::binary};
    if (!inStream)
    {
      throw std::invalid_argument("Config parser: file for parsing doesn't exist or is empty");
    }

    fillFromStream(inStream);
  }

  //----------------------------------------------------------------------------

  bool Parser::hasSection(const string& secName) const
  {
    auto it = content.find(secName);
    return (it != content.end());
  }

  //----------------------------------------------------------------------------

  bool Parser::hasKey(const string& secName, const string& keyName) const
  {
    auto secIt = content.find(secName);
    if (secIt == content.end()) return false;

    const KeyValueMap& sec = secIt->second;

    auto it = sec.find(keyName);
    return (it != sec.end());
  }

  //----------------------------------------------------------------------------

  bool Parser::hasKey(const string& keyName) const
  {
    return hasKey(defaultSectionName, keyName);
  }

  //----------------------------------------------------------------------------

  optional<estring> Parser::getValue(const string& secName, const string& keyName) const
  {
    auto secIt = content.find(secName);
    if (secIt == content.end())
    {
      return optional<estring>{};
    }

    const KeyValueMap& sec = secIt->second;

    auto it = sec.find(keyName);
    if (it == sec.end())
    {
      return optional<estring>{};
    }

    return optional<estring>{it->second};
  }

  //----------------------------------------------------------------------------

  optional<estring> Parser::getValue(const string& keyName) const
  {
    return getValue(defaultSectionName, keyName);
  }

  //----------------------------------------------------------------------------

  optional<bool> Parser::getValueAsBool(const string& secName, const string& keyName) const
  {
    optional<estring> s = getValue(secName, keyName);
    if (!s.has_value())
    {
      return optional<bool>{};
    }

    s->toLower();
    for (const string& val : {"1", "true", "on", "yes"})
    {
      if (*s == val)
      {
        return true;
      }
    }
    for (const string& val : {"0", "false", "off", "no"})
    {
      if (*s == val)
      {
        return false;
      }
    }

    return optional<bool>{};
  }

  //----------------------------------------------------------------------------

  optional<bool> Parser::getValueAsBool(const string& keyName) const
  {
    return getValueAsBool(defaultSectionName, keyName);
  }

  //----------------------------------------------------------------------------

  optional<int> Parser::getValueAsInt(const string& secName, const string& keyName) const
  {
    optional<estring> s = getValue(secName, keyName);
    if (!s.has_value())
    {
      return optional<int>{};
    }

    s->trim();
    if (!s->isInt())
    {
      return optional<int>{};
    }

    return optional<int>{stoi(*s)};
  }

  //----------------------------------------------------------------------------

  optional<int> Parser::getValueAsInt(const string& keyName) const
  {
    return getValueAsInt(defaultSectionName, keyName);
  }

  //----------------------------------------------------------------------------

  bool Parser::checkConstraint(const string& keyName, ValueConstraint c, string* errMsg) const
  {
    return checkConstraint(defaultSectionName, keyName, c, errMsg);
  }

  //----------------------------------------------------------------------------

  bool Parser::checkConstraint(const string& secName, const string& keyName, ValueConstraint c, string* errMsg) const
  {
    if (keyName.empty())
    {
      throw std::invalid_argument("ConfigFileParser constraint check: received empty key name!");
    }

    if (secName.empty())
    {
      throw std::invalid_argument("ConfigFileParser constraint check: received empty section name!");
    }

    // a helper function for preparing the first part of an error message
    auto prepErrMsg = [&secName, &keyName]() {
      estring msg{"The key %1 %2 "};
      msg.arg(keyName);
      if (secName != defaultSectionName) msg.arg("in section " + secName);
      else msg.arg("");

      return msg;
    };

    // get the value as an optional string and
    // leave all further checks to the
    // separate constraint checker
    optional<estring> v = getValue(secName, keyName);
    if (!Sloppy::checkConstraint(v, c, errMsg))
    {
      if (errMsg != nullptr)
      {
        *errMsg = prepErrMsg() + *errMsg;
      }

      return false;
    }

    return true;
  }

  //----------------------------------------------------------------------------

  bool Parser::checkConstraint(const ConstraintCheckData& ccd, string* errMsg) const
  {
    if (ccd.secName.empty())
    {
      return checkConstraint(defaultSectionName, ccd.keyName, ccd.c, errMsg);
    }

    return checkConstraint(ccd.secName, ccd.keyName, ccd.c, errMsg);
  }

  //----------------------------------------------------------------------------

  bool Parser::bulkCheckConstraints(vector<ConstraintCheckData> constraintList, bool logErrorToConsole, string* errMsg) const
  {
    string myErr;

    for (const ConstraintCheckData& ccd : constraintList)
    {
      bool isOkay = checkConstraint(ccd, &myErr);

      if (!isOkay)
      {
        if (logErrorToConsole)
        {
          cerr << endl << myErr << endl;
        }
        if (errMsg != nullptr) *errMsg = myErr;

        return false;
      }
    }

    return true;
  }

  //----------------------------------------------------------------------------

  bool Parser::checkConstraint_IntRange(const string& secName, const string& keyName, optional<int> minVal, optional<int> maxVal, string* errMsg) const
  {
    bool hasMin = minVal.has_value();
    bool hasMax = maxVal.has_value();

    if (hasMin && hasMax)
    {
      if (*maxVal < *minVal)
      {
        throw std::range_error("Config file parser: parameters for integer range check are inconsistent.");
      }
    }

    bool isOkay = checkConstraint(secName, keyName, ValueConstraint::Integer, errMsg);
    if (!isOkay) return false;

    // prepare a helper functions that puts keyname and
    // an optional section name into the first two parameters
    // of a error message
    auto prepErrMsg = [&secName, &keyName]() {
      estring msg{"The key %1 %2 "};
      msg.arg(keyName);
      if (secName != defaultSectionName) msg.arg("in section " + secName);
      else msg.arg("");

      return msg;
    };

    int v = getValueAsInt(secName, keyName).value();

    if (hasMin)
    {
      isOkay = (v >= *minVal);
      if (!isOkay && (errMsg != nullptr))
      {
        estring e = prepErrMsg();
        e += "shall have a min value of at least %1";
        e.arg(*minVal);
        *errMsg = e;
      }

      if (!isOkay) return false;
    }

    if (hasMax)
    {
      isOkay = (v <= *maxVal);
      if (!isOkay && (errMsg != nullptr))
      {
        estring e = prepErrMsg();
        e += "shall have a max value of not more than %1";
        e.arg(*maxVal);
        *errMsg = e;
      }

      if (!isOkay) return false;
    }

    return true;
  }

  //----------------------------------------------------------------------------

  bool Parser::checkConstraint_IntRange(const string& keyName, optional<int> minVal, optional<int> maxVal, string* errMsg) const
  {
    return checkConstraint_IntRange(defaultSectionName, keyName, minVal, maxVal, errMsg);
  }

  //----------------------------------------------------------------------------

  bool Parser::checkConstraint_StrLen(const string& secName, const string& keyName, optional<size_t> minLen, optional<size_t> maxLen, string* errMsg) const
  {
    bool hasMin = minLen.has_value();
    bool hasMax = maxLen.has_value();

    if (hasMin && hasMax)
    {
      if (*maxLen < *minLen)
      {
        throw std::range_error("Config file parser: parameters for string length check are inconsistent.");
      }
    }

    bool isOkay = checkConstraint(secName, keyName, ValueConstraint::NotEmpty, errMsg);
    if (!isOkay) return false;

    // prepare a helper functions that puts keyname and
    // an optional section name into the first two parameters
    // of a error message
    auto prepErrMsg = [&secName, &keyName]() {
      estring msg{"The key %1 %2 "};
      msg.arg(keyName);
      if (secName != defaultSectionName) msg.arg("in section " + secName);
      else msg.arg("");

      return msg;
    };

    const estring& v = getValue(secName, keyName).value();

    if (hasMin && (*minLen > 0))
    {
      isOkay = (v.length() >= *minLen);
      if (!isOkay && (errMsg != nullptr))
      {
        estring e = prepErrMsg();
        e += "shall have a min length of at least %1 characters!";
        e.arg(*minLen);
        *errMsg = e;
      }

      if (!isOkay) return false;
    }

    if (hasMax && (*maxLen > 0))
    {
      isOkay = (v.length() <= *maxLen);
      if (!isOkay && (errMsg != nullptr))
      {
        estring e = prepErrMsg();
        e += "shall have a max length of not more than %1 characters!";
        e.arg(*maxLen);
        *errMsg = e;
      }

      if (!isOkay) return false;
    }

    return true;
  }

  //----------------------------------------------------------------------------

  bool Parser::checkConstraint_StrLen(const string& keyName, optional<size_t> minLen, optional<size_t> maxLen, string* errMsg) const
  {
    return checkConstraint_StrLen(defaultSectionName, keyName, minLen, maxLen, errMsg);
  }

  //----------------------------------------------------------------------------

  vector<string> Parser::allSections() const
  {
    vector<string> result;

    for (const auto& p : content)
    {
      result.push_back(p.first);
    }

    return result;
  }

  //----------------------------------------------------------------------------

  void Parser::fillFromStream(istream& inStream)
  {
    // prepare a few regex; do this only once because
    // construction of regexs is expensive
    static const regex reSection{"^\\[([^\\]]*)\\]"};
    static const regex reData{"(^[_[:alnum:]]+[^=]*)=(.*)"};

    // add a container (map) for the content of the default section
    content.emplace(defaultSectionName, KeyValueMap());

    estring curSecName = defaultSectionName;

    // read the data line by line
    while (inStream)   // used as a condition, the stream is evaluated to 'false' if EOF or errors occur
    {
      estring line;
      getline(inStream, line);

      // strip white spaces
      line.trim();

      // ignore empty lines
      if (line.empty()) continue;

      // ignore comments
      if (line.startsWith("#") || line.startsWith(";")) continue;

      // check for a new section
      smatch sm;
      if (regex_search(line, sm, reSection))
      {
        curSecName = sm[1];
        curSecName.trim();
        getOrCreateSection(curSecName);
        continue;
      }

      // check for key-value-pairs
      if (regex_search(line, sm, reData))
      {
        estring k{sm[1]};
        estring v{sm[2]};

        // strip surrounding white spaces from the key and the value.
        // if this leads to an empty value that's fine
        k.trim();
        v.trim();

        // insert the new value
        insertOrOverwriteValue(curSecName, k, v);
        continue;
      }

      // log an error
      cerr << "The following text line was ill-formated: " << line << endl;
    }
  }

  //----------------------------------------------------------------------------

  KeyValueMap& Parser::getOrCreateSection(const string& secName)
  {
    if (secName.empty())
    {
      throw invalid_argument("Cannot insert section with empty name!");
    }

    auto result = content.emplace(secName, KeyValueMap());

    // return either an existing section of "secName" or the newly
    // created section
    return (result.first)->second;
  }

  //----------------------------------------------------------------------------

  void Parser::insertOrOverwriteValue(const string& secName, const string& keyName, const string& val)
  {
    insertOrOverwriteValue(getOrCreateSection(secName), keyName, val);
  }

  //----------------------------------------------------------------------------

  void Parser::insertOrOverwriteValue(KeyValueMap& sec, const string& keyName, const string& val)
  {
    auto result = sec.emplace(keyName, val);

    // overwrite if the key already existed
    if (!(result.second))
    {
      auto& existingPair = *(result.first);
      existingPair.second = val;
    }
  }

  //----------------------------------------------------------------------------

}
