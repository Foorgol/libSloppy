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
    SyntaxTree::SyntaxTree()
      :reToken{R"(\{\{\s*([^\}\{]+)\s*\}\})"},
        reFor{R"(for ([\w|.|:]+)\s*:\s*([\w|.|:]+))"},
        reIf{R"(if (!?)\s*([\w|.|:]+))"},
        reVar{R"([\w|.|:]+)"}
    {
    }

    //----------------------------------------------------------------------------

    SyntaxTreeError SyntaxTree::parse(const string& s)
    {
      tree.clear();

      if (s.empty()) return SyntaxTreeError{};

      // a few status vars for the parser
      size_t curSectionStart = 0;
      size_t idxParentItem = InvalidIndex;
      SyntaxSectionType curSection = SyntaxSectionType::Root;

      // a flag that indicates whether we have to update the parent's
      // link to its first child
      bool updateParent = false;

      // a flag that indicates whether we have already inserted items
      // in this block or not
      size_t idxLastSibling = InvalidIndex;

      // a helper function that updates the link information in the
      // list AFTER we've inserted a new list item
      auto updateLinks = [&]()
      {
        size_t idxInsertedItem = tree.size() - 1;

        SyntaxTreeItem& newItem = tree.back();
        newItem.idxNextSibling = InvalidIndex;
        newItem.idxFirstChild = InvalidIndex;
        newItem.idxParent = idxParentItem;

        if (updateParent)
        {
          SyntaxTreeItem& parent = tree.at(idxParentItem);
          parent.idxFirstChild = idxInsertedItem;
          updateParent = false;
        }

        if (idxLastSibling != InvalidIndex)
        {
          SyntaxTreeItem& sib = tree.at(idxLastSibling);
          sib.idxNextSibling = idxInsertedItem;
        }

        idxLastSibling = idxInsertedItem;
      };

      // a helper function that links all subsequent new item
      // to the most recently inserted item. Means: it makes the
      // last item in the list the parent of all subsequent items
      //
      // this one has to be called AFTER updateLinks()
      auto levelDown = [&]()
      {
        idxParentItem = tree.size() - 1;  // last item becomes parent
        updateParent = true;
        idxLastSibling = InvalidIndex;

        SyntaxTreeItem& newParent = tree.back();
        if (newParent.t == SyntaxTreeItemType::ForLoop)
        {
          curSection = SyntaxSectionType::ForLoop;
        } else {
          curSection = SyntaxSectionType::Condition;
        }
      };

      // a helper function that goes one level up in the tree.
      //
      // this one has to be called AFTER updateLinks()
      auto levelUp = [&]()
      {
        if (idxParentItem == InvalidIndex) return;   // we are already at top level

        SyntaxTreeItem& parent = tree.at(idxParentItem);
        idxLastSibling = idxParentItem;

        // the old parent's parent becomes the new parent
        idxParentItem = parent.idxParent;

        // no need for any linking updates of the parent because
        // we jump back to already inserted and linked items
        updateParent = false;

        // determine the section type, depending on the new parent
        if (idxParentItem == InvalidIndex)
        {
          curSection = SyntaxSectionType::Root;
        } else {
          SyntaxTreeItem& newParent = tree.at(idxParentItem);  // this is the NEW parent, one level up
          if (newParent.t == SyntaxTreeItemType::ForLoop)
          {
            curSection = SyntaxSectionType::ForLoop;
          } else {
            curSection = SyntaxSectionType::Condition;
          }
        }
      };

      // find all tokens
      sregex_iterator itTokens{s.begin(), s.end(), reToken};

      // iterate over all tokens and create section / tree items as necessary
      for (auto it = itTokens ; it != sregex_iterator{}; ++it)
      {
        // derefence into a submatch to make code more readable
        auto& sm = *it;

        // is there an unhandled section before the current match?
        // if yes, it MUST be static data
        if (sm.position() != curSectionStart)
        {
          SyntaxTreeItem sti;
          sti.t = SyntaxTreeItemType::Static;
          sti.idxFirstChar = curSectionStart;
          sti.idxLastChar = sm.position() - 1;
          tree.push_back(sti);
          updateLinks();
        }

        // extract the inner token, trim white spaces around it
        // and skip it if it's empty
        string token = sm[1];
        if (!(trimAndCheckString(token)))
        {
          // set the start of the next section to one
          // character after the closing bracket of the token
          curSectionStart = sm.position() + sm.size();
          continue;
        }

        // determine the token type and perform a first
        // syntax check
        TokenType tt;
        SyntaxTreeError err;
        tie(tt, err) = doSyntaxCheck(token, curSection);
        if (err.isError())
        {
          err.updatePosition(sm);
          return err;
        }

        //
        // at this point we can safely process the token
        //

        // if this is only an end-token, we don't have to create
        // a new tree item. we just go one level up and process
        // the next item
        if ((tt == TokenType::EndFor) || (tt == TokenType::EndIf))
        {
          levelUp();

          // set the start of the next section to one
          // character after the closing bracket of the token
          curSectionStart = sm.position() + sm.str().size();

          continue;
        }

        // prepare a new element for the tree
        SyntaxTreeItem sti;
        sti.idxFirstChar = sm.position();
        sti.idxLastChar = sti.idxFirstChar + sm.str().size() - 1;

        if (tt == TokenType::Variable)
        {
          sti.t = SyntaxTreeItemType::Variable;
          sti.varName = token;
        }

        if (tt == TokenType::StartIf)
        {
          smatch sm;
          regex_match(token, sm, reIf);

          sti.t = SyntaxTreeItemType::Condition;
          sti.varName = sm[2];
          sti.invertCondition = (sm[1] == "!");
        }

        if (tt == TokenType::StartFor)
        {
          smatch sm;
          regex_match(token, sm, reFor);

          sti.t = SyntaxTreeItemType::ForLoop;
          sti.varName = sm[1];
          sti.listName = sm[2];
        }

        // store the new item and adjust the levels, if necessary
        tree.push_back(sti);
        updateLinks();
        if ((tt == TokenType::StartFor) || (tt == TokenType::StartIf)) levelDown();

        // set the start of the next section to one
        // character after the closing bracket of the token
        curSectionStart = sm.position() + sm.str().size();
      }

      // after we have processed all tokens, we MUST be back at root
      // level, otherwise the file is inconsistent
      if (curSection != SyntaxSectionType::Root)
      {
        SyntaxTreeItem& unmatchedParent = tree.at(idxParentItem);
        string msg = "Unmatched %1-section starting at position %2";

        if (unmatchedParent.t == SyntaxTreeItemType::ForLoop) strArg(msg, "for");
        else strArg(msg, "if");

        strArg(msg, (int)unmatchedParent.idxFirstChar);

        SyntaxTreeError err;
        err.idxFirstChar = unmatchedParent.idxFirstChar;
        err.idxLastChar = 0;  // dummy value
        err.msg = msg;
        return err;
      }

      // handle the remaining text after the last token, if any
      if (curSectionStart < s.size())
      {
        SyntaxTreeItem sti;
        sti.t = SyntaxTreeItemType::Static;
        sti.idxFirstChar = curSectionStart;
        sti.idxLastChar = s.size() - 1;
        tree.push_back(sti);
        updateLinks();
      }

      return SyntaxTreeError{};
    }

    //----------------------------------------------------------------------------

    tuple<TokenType, bool> SyntaxTree::checkToken(const string& token) const
    {
      int rc = isValid_endif(token);
      if (rc == 1) return make_tuple(TokenType::EndIf, true);
      if (rc < 0) return make_tuple(TokenType::EndIf, false);

      rc = isValid_if(token);
      if (rc == 1) return make_tuple(TokenType::StartIf, true);
      if (rc < 0) return make_tuple(TokenType::StartIf, false);

      rc = isValid_endfor(token);
      if (rc == 1) return make_tuple(TokenType::EndFor, true);
      if (rc < 0) return make_tuple(TokenType::EndFor, false);

      rc = isValid_for(token);
      if (rc == 1) return make_tuple(TokenType::StartFor, true);
      if (rc < 0) return make_tuple(TokenType::StartFor, false);

      // if no keyword match, it must be a valid variable
      return make_tuple(TokenType::Variable, isValid_var(token));
    }

    //----------------------------------------------------------------------------

    tuple<TokenType, SyntaxTreeError> SyntaxTree::doSyntaxCheck(const string& token, SyntaxSectionType secType) const
    {
      TokenType tt;
      bool isValid;
      tie(tt, isValid) = checkToken(token);

      // report syntactically invalid tokens
      if (!isValid)
      {
        string msg = "Syntax error in '%1' token";
        switch (tt)
        {
        case TokenType::StartIf:
          strArg(msg, "if");
          break;

        case TokenType::EndIf:
          strArg(msg, "endif");
          break;

        case TokenType::StartFor:
          strArg(msg, "for");
          break;

        case TokenType::EndFor:
          strArg(msg, "endfor");
          break;

        default:
          msg = "Invalid variable name";
        }

        return make_tuple(tt, SyntaxTreeError{msg});
      }

      // make sure that "endif" and "endfor" match the current
      // section type
      if ((tt == TokenType::EndIf) && (secType != SyntaxSectionType::Condition))
      {
        return make_tuple(tt, SyntaxTreeError{"Unexpected 'endif' token outside an if-section"});
      }
      if ((tt == TokenType::EndFor) && (secType != SyntaxSectionType::ForLoop))
      {
        return make_tuple(tt, SyntaxTreeError{"Unexpected 'endfor' token outside a for-loop"});
      }

      return make_tuple(tt, SyntaxTreeError{});
    }

    //----------------------------------------------------------------------------

    int SyntaxTree::isValid_endif(const string& token) const
    {
      // check if the token starts with "endif"
      if (token.substr(0, 5) != "endif") return 0;

      // okay, it's an "endif". In this case, the token
      // may not contain any additional characters
      return (token == "endif") ? 1 : -1;
    }

    //----------------------------------------------------------------------------

    int SyntaxTree::isValid_if(const string& token) const
    {
      // check if the token starts with "if"
      if (token.substr(0, 2) != "if") return 0;

      // okay, it's an "if". In this case, the token
      // must match the regular expression for ifs
      return (regex_match(token, reIf)) ? 1 : -1;
    }

    //----------------------------------------------------------------------------

    int SyntaxTree::isValid_endfor(const string& token) const
    {
      // check if the token starts with "endfor"
      if (token.substr(0, 6) != "endfor") return 0;

      // okay, it's an "endfor". In this case, the token
      // may not contain any additional characters
      return (token == "endfor") ? 1 : -1;
    }

    //----------------------------------------------------------------------------

    int SyntaxTree::isValid_for(const string& token) const
    {
      // check if the token starts with "for"
      if (token.substr(0, 3) != "for") return 0;

      // okay, it's an "for". In this case, the token
      // must match the regular expression for fors
      return (regex_match(token, reFor)) ? 1 : -1;
    }

    //----------------------------------------------------------------------------

    bool SyntaxTree::isValid_var(const string& token) const
    {
      return regex_match(token, reVar);
    }

    //----------------------------------------------------------------------------

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
