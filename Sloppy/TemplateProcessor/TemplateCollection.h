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
