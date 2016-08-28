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
      unique_ptr<Template> fromFile(const string& fName);
      void getSubstitutedData(string& outString, const SubstDic& dic=SubstDic{}, const string& keyPrefix="", const string& keyPostfix="") const;
      void applyPermanentSubstitution(const SubstDic& dic, const string& keyPrefix="", const string& keyPostfix="");
      void doForEachLoop(string& outString, SubstDicList dl, const string& keyPrefix="", const string& keyPostfix="", const string& delim="") const;

    protected:
      string data;
    };
  }
}

#endif
