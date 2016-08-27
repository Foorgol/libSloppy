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

    void Template::getSubstitutedData(const SubstDic& dic, string& outString, const string& keyPrefix, const string& keyPostfix) const
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


    //----------------------------------------------------------------------------

  }
}
