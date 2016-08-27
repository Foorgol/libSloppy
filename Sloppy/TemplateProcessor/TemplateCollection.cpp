#include <fstream>

#include <boost/filesystem.hpp>

#include "TemplateCollection.h"
#include "../libSloppy.h"

namespace Sloppy
{
  namespace TemplateProcessor
  {

    TemplateCollection::TemplateCollection(const string& dataDir)
    {
      throw runtime_error("Not yet implemented!");
    }

    //----------------------------------------------------------------------------

    bool TemplateCollection::addTemplate(const string& shortName, istream& inData)
    {
      //auto result = store.emplace(piecewise_construct, forward_as_tuple(shortName), forward_as_tuple(inData));
      auto result = store.emplace(shortName, inData);

      return result.second;
    }

    //----------------------------------------------------------------------------

    bool TemplateCollection::addTemplate(const string& shortName, const string& inData)
    {
      auto result = store.emplace(shortName, inData);

      return result.second;
    }

    //----------------------------------------------------------------------------

    bool TemplateCollection::addTemplateFromFile(const string& shortName, const string& fName)
    {
      ifstream f{fName, ios::binary};
      if (!f) return false;

      return addTemplate(shortName, f);
    }

    //----------------------------------------------------------------------------

    bool TemplateCollection::removeTemplate(const string& shortName)
    {
      return (store.erase(shortName) != 0);
    }

    //----------------------------------------------------------------------------

    bool TemplateCollection::replaceTemplate(const string& shortName, istream& inData)
    {
      bool isOk = removeTemplate(shortName);
      if (!isOk) return false;
      return addTemplate(shortName, inData);
    }

    //----------------------------------------------------------------------------

    bool TemplateCollection::replaceTemplate(const string& shortName, const string& inData)
    {
      bool isOk = removeTemplate(shortName);
      if (!isOk) return false;
      return addTemplate(shortName, inData);
    }

    //----------------------------------------------------------------------------

    void TemplateCollection::applyPermanentSubstitution(const string& shortName, const SubstDic& dic, const string& keyPrefix, const string& keyPostfix)
    {
      auto it = store.find(shortName);
      if (it == store.end()) return;

      it->second.applyPermanentSubstitution(dic, keyPrefix, keyPostfix);
    }

    //----------------------------------------------------------------------------

    bool TemplateCollection::getSubstitutedData(const string& shortName, string& outString, const SubstDic& dic, const string& keyPrefix, const string& keyPostfix) const
    {
      auto it = store.find(shortName);
      if (it == store.end()) return false;

      const Template& tmpl = it->second;

      tmpl.getSubstitutedData(outString, dic, keyPrefix, keyPostfix);

      return true;
    }


    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------

  }
}
