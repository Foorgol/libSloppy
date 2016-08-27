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

      bool addTemplate(istream& inData, const string& shortName);
      bool addTemplate(const string& inData, const string& shortName);
      bool addTemplateFromFile(const string& fName, const string& shortName);

      bool getSubstitutedData(const string& tName, const SubstDic& dic, string& outString, const string& keyPrefix="", const string& keyPostfix="") const;

    protected:
      TemplateStore store;
    };
  }
}

#endif
