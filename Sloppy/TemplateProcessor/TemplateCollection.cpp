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

    bool TemplateCollection::addTemplate(istream& inData, const string& shortName)
    {
      //auto result = store.emplace(piecewise_construct, forward_as_tuple(shortName), forward_as_tuple(inData));
      auto result = store.emplace(shortName, inData);

      return result.second;
    }

    //----------------------------------------------------------------------------

    bool TemplateCollection::addTemplate(const string& inData, const string& shortName)
    {
      auto result = store.emplace(shortName, inData);

      return result.second;
    }

    //----------------------------------------------------------------------------

    bool TemplateCollection::addTemplateFromFile(const string& fName, const string& shortName)
    {
      ifstream f{fName, ios::binary};
      if (!f) return false;

      return addTemplate(f, shortName);
    }

    //----------------------------------------------------------------------------

    bool TemplateCollection::removeTemplate(const string& shortName)
    {
      return (store.erase(shortName) != 0);
    }

    //----------------------------------------------------------------------------

    bool TemplateCollection::getSubstitutedData(const string& tName, const SubstDic& dic, string& outString, const string& keyPrefix, const string& keyPostfix) const
    {
      auto it = store.find(tName);
      if (it == store.end()) return false;

      const Template& tmpl = it->second;

      tmpl.getSubstitutedData(dic, outString, keyPrefix, keyPostfix);

      return true;
    }


    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------

  }
}
