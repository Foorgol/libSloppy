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
#include <iostream>

#include <boost/filesystem.hpp>

#include "TemplateSys.h"
#include "../libSloppy.h"

namespace bfs = boost::filesystem;

namespace Sloppy
{
  namespace TemplateSystem
  {

    RawTemplate::RawTemplate(istream& inData)
    // copy the complete stream until EOF into
    // an internal buffer
      :data{std::istreambuf_iterator<char>{inData}, {}}
    {
    }

    //----------------------------------------------------------------------------

    RawTemplate::RawTemplate(const string& inData)
      :data{inData}
    {
    }

    //----------------------------------------------------------------------------

    unique_ptr<RawTemplate> RawTemplate::fromFile(const string& fName)
    {
      ifstream f{fName, ios::binary};
      if (!f) return nullptr;

      return make_unique<RawTemplate>(f);
    }

    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    TemplateStore::TemplateStore(const string& rootDir, const StringList& extList)
    {
      bfs::path rootPath{rootDir};
      if (!(bfs::exists(rootPath)))
      {
        throw std::invalid_argument("TemplateStore initialized with invalid base dir");
      }
      if (!(bfs::is_directory(rootPath)))
      {
        throw std::invalid_argument("TemplateStore initialized with invalid base dir");
      }


      // recurse through the directory and get a list of all files
      StringList allFiles = Sloppy::getAllFilesInDirTree(rootDir, false);

      // if we have a list of valid file extensions,
      // keep only those files that match the extension
      if (!(extList.empty()))
      {
        auto it = allFiles.begin();
        while (it != allFiles.end())
        {
          bfs::path p{*it};
          string ext = p.extension().native();
          if ((!(ext.empty())) && (ext[0] == '.'))
          {
            ext = ext.substr(1);
          }

          if (!(isInVector<string>(extList, ext)))
          {
            it = allFiles.erase(it);
          } else {
            ++it;
          }
        }
      }

      //
      // at this point we have a list of all valid files in our store
      //

      // stop here if there are no files in the list
      if (allFiles.empty())
      {
        throw std::invalid_argument("TemplateStore found no files in root dir!");
      }

      // read all files and store them as raw data in a map
      // with the relative file name as key
      for (const string& p : allFiles)
      {
        ifstream f{p, ios::binary};
        if (!f)
        {
          cerr << "TemplateStore: could not read " << p << "\n";
          continue;
        }

        string relPath = bfs::path{p}.lexically_relative(rootPath).native();
        rawData.emplace(relPath, RawTemplate{f});
      }

      // finally, we should have at least one template
      if (rawData.empty())
      {
        throw std::invalid_argument("TemplateStore could not read/parse any file!");
      }
    }
  }



  //----------------------------------------------------------------------------

}
