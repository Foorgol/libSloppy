#ifndef SLOPPY__TEMPLATE_H
#define SLOPPY__TEMPLATE_H

#include <istream>
#include <unordered_map>
#include <memory>

using namespace std;

namespace Sloppy
{
  namespace TemplateProcessor
  {
    using SubstDic = unordered_map<string, string>;

    class Template
    {
    public:
      explicit Template(istream& inData);
      explicit Template(const string& inData);
      unique_ptr<Template> fromFile(const string& fName);
      void getSubstitutedData(const SubstDic& dic, string& outString, const string& keyPrefix="", const string& keyPostfix="") const;

    protected:
      string data;
    };
  }
}

#endif
