#include <string>
#include <regex>
#include <iostream>
#include <sstream>
#include <fstream>

#include <boost/algorithm/string.hpp>
#include "ConfigFileParser.h"

namespace Sloppy
{
  namespace ConfigFileParser
  {

    unique_ptr<Parser> Parser::read(istream& inStream)
    {
      unique_ptr<Parser> result = unique_ptr<Parser>{new Parser};

      // prepare a few regex
      regex reSection{"^\\[([^\\]]*)\\]"};
      regex reData{"(^[_[:alnum:]]+[^=]*)=(.*)"};

      //SectionData& curSec = result->getOrCreateSection(defaultSectionName);
      string curSecName = defaultSectionName;

      // read the file line by line
      while (inStream)   // used as a condition, the stream is evaluated to 'false' if EOF or errors occur
      {
        string line;
        getline(inStream, line);

        // strip white spaces
        boost::trim(line);

        // ignore empty lines
        if (line.empty()) continue;

        // ignore comments
        if (boost::starts_with(line, "#")) continue;
        if (boost::starts_with(line, ";")) continue;

        // check for a new section
        smatch sm;
        if (regex_search(line, sm, reSection))
        {
          curSecName = sm[1];
          boost::trim(curSecName);
          result->getOrCreateSection(curSecName);
          continue;
        }

        // check for key-value-pairs
        if (regex_search(line, sm, reData))
        {
          string k = sm[1];
          string v = sm[2];

          // strip surrounding white spaces from the key and the value.
          // if this leads to an empty value that's fine
          boost::trim(k);
          boost::trim(v);

          // insert the new value
          result->insertOrOverwriteValue(curSecName, k, v);
          continue;
        }

        // log an error
        cerr << "The following text line was ill-formated: " << line << endl;
      }

      return result;
    }

    //----------------------------------------------------------------------------

    unique_ptr<Parser> Parser::read(const string& s)
    {
      istringstream ss{s};
      return read(ss);
    }

    //----------------------------------------------------------------------------

    unique_ptr<Parser> Parser::readFromFile(const string& fName)
    {
      ifstream f{fName, ios::binary};
      if (!f) return nullptr;

      return read(f);
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

      const SectionData& sec = secIt->second;

      auto it = sec.find(keyName);
      return (it != sec.end());
    }

    //----------------------------------------------------------------------------

    bool Parser::hasKey(const string& keyName) const
    {
      return hasKey(defaultSectionName, keyName);
    }

    //----------------------------------------------------------------------------

    string Parser::getValue(const string& secName, const string& keyName, bool* isOk) const
    {
      auto secIt = content.find(secName);
      if (secIt == content.end())
      {
        assignIfNotNull<bool>(isOk, false);
        return "";
      }

      const SectionData& sec = secIt->second;

      auto it = sec.find(keyName);
      if (it == sec.end())
      {
        assignIfNotNull<bool>(isOk, false);
        return "";
      }

      assignIfNotNull<bool>(isOk, true);
      return it->second;
    }

    //----------------------------------------------------------------------------

    string Parser::getValue(const string& keyName, bool* isOk) const
    {
      return getValue(defaultSectionName, keyName, isOk);
    }

    //----------------------------------------------------------------------------

    Parser::Parser()
    {
      content.emplace(defaultSectionName, SectionData());
    }

    //----------------------------------------------------------------------------

    SectionData& Parser::getOrCreateSection(const string& secName)
    {
      if (secName.empty())
      {
        throw invalid_argument("Cannot insert section with empty name!");
      }

      auto result = content.emplace(secName, SectionData());

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

    void Parser::insertOrOverwriteValue(SectionData& sec, const string& keyName, const string& val)
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
}
