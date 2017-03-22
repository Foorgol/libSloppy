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
#include "../json/json.h"

namespace bfs = boost::filesystem;

namespace Sloppy
{
  namespace TemplateSystem
  {
    SyntaxTree::SyntaxTree()
      :reToken{R"(\{\{\s*([^\}\{]+)\s*\}\})"},
        reFor{R"(for ([\w|.|:]+)\s*:\s*([\w|.|:]+))"},
        reIf{R"(if (!?)\s*([\w|.|:]+))"},
        reVar{R"([\w|.|:]+)"},
        reInclude{R"(include ([\w|.|:|/]+))"}
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
          size_t len = sti.idxLastChar - sti.idxFirstChar + 1;
          sti.staticText = s.substr(sti.idxFirstChar, len);
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

        if (tt == TokenType::IncludeCmd)
        {
          smatch sm;
          regex_match(token, sm, reInclude);

          sti.t = SyntaxTreeItemType::IncludeCmd;
          sti.varName = sm[1];
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
        size_t len = sti.idxLastChar - sti.idxFirstChar + 1;
        sti.staticText = s.substr(sti.idxFirstChar, len);
        tree.push_back(sti);
        updateLinks();
      }

      return SyntaxTreeError{};
    }

    //----------------------------------------------------------------------------

    StringList SyntaxTree::getIncludes() const
    {
      StringList result;

      for (const SyntaxTreeItem& sti : tree)
      {
        if (sti.t != SyntaxTreeItemType::IncludeCmd) continue;
        result.push_back(sti.varName);
      }

      return result;
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

      rc = isValid_include(token);
      if (rc == 1) return make_tuple(TokenType::IncludeCmd, true);
      if (rc < 0) return make_tuple(TokenType::IncludeCmd, false);

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

        case TokenType::IncludeCmd:
          strArg(msg, "include");
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

    int SyntaxTree::isValid_include(const string& token) const
    {
      // check if the token starts with "include"
      if (token.substr(0, 7) != "include") return 0;

      // okay, it's an "include". In this case, the token
      // must match the regular expression for include
      return (regex_match(token, reInclude)) ? 1 : -1;
    }

    //----------------------------------------------------------------------------

    bool SyntaxTree::isValid_var(const string& token) const
    {
      return regex_match(token, reVar);
    }

    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    Template::Template(istream& inData)
    // copy the complete stream until EOF into
    // an internal buffer
      :rawData{std::istreambuf_iterator<char>{inData}, {}}, syntaxOkay{false}
    {
    }

    //----------------------------------------------------------------------------

    Template::Template(const string& inData)
      :rawData{inData}, syntaxOkay{false}
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

    SyntaxTreeError Template::parse()
    {
      auto err = st.parse(rawData);
      syntaxOkay = !(err.isError());

      return err;
    }

    //----------------------------------------------------------------------------

    StringList Template::getIncludes() const
    {
      if (!syntaxOkay) return StringList{};

      return st.getIncludes();
    }

    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    TemplateStore::TemplateStore(const string& rootDir, const StringList& extList)
      :langCode{}
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

        // parse the file and make sure it's syntactically correct
        Template t{f};
        auto err = t.parse();
        if (!(t.isSyntaxOkay()))
        {
          cerr << "TemplateStore: error parsing " << p << ": " << err.str() << "\n";
          continue;
        }

        string relPath = bfs::path{p}.lexically_relative(rootPath).native();
        docs.emplace(relPath, t);
      }

      // finally, we should have at least one template
      if (docs.empty())
      {
        throw std::invalid_argument("TemplateStore could not read/parse any file!");
      }
    }

    //----------------------------------------------------------------------------

    string TemplateStore::get(const string& tName, const Json::Value& dic)
    {
      StringList visited;

      return getTemplate_Recursive(tName, dic, visited);
    }

    //----------------------------------------------------------------------------

    string TemplateStore::getLocalizedTemplateName(const string& docName) const
    {
      // remove a trailing '/' if any
      string d{docName};
      if (d[0] == '/') d = d.substr(1);

      // does the document exist at all?
      if (docs.find(d) == docs.end()) return string{};

      // if no language is set, we use the default version
      if (langCode.empty()) return d;

      // prepend the docName with the language code and see
      // if such a localized version exists
      string localName = langCode + "/" + d;
      return (docs.find(localName) == docs.end()) ? d : localName;
    }

    //----------------------------------------------------------------------------

    string TemplateStore::getTemplate_Recursive(const string& tName, const Json::Value& dic, StringList& visitedTemplates) const
    {
      string localTemplate = getLocalizedTemplateName(tName);
      if (localTemplate.empty())
      {
        throw std::invalid_argument("TemplateStore: non-existing template requested!");
      }

      // have we used this template before? are we in a circular include dependency?
      if (isInVector<string>(visitedTemplates, localTemplate))
      {
        throw std::runtime_error("TemplateStore: circular include-dependency in templates!");
      }

      // take a note that we're now processing this particular template
      visitedTemplates.push_back(localTemplate);

      // iterate over the elements of the syntax tree
      auto it = docs.find(localTemplate);  // this template is guaranteed to exist, that was checked above
      auto allTreeItems = it->second.getTreeAsRef();
      if (allTreeItems.empty())
      {
        return string{};
      }

      // prepare a hash that maps "local for variables" to the
      // associated JSON-subtree
      unordered_map<string, const Json::Value&> localScopeVars;

      string result = getSyntaxSubtree(allTreeItems, 0, dic, localScopeVars, visitedTemplates);

      // remove our "tag" from the stack of visited templates.
      // if we would not do this, it wouldn't be possible to include a template
      // more than once in the same document or more than once from different, unrelated
      // documents
      visitedTemplates.pop_back();

      return result;
    }

    //----------------------------------------------------------------------------

    string TemplateStore::getSyntaxSubtree(const SyntaxTreeItemList& tree, size_t idxFirstItem, const Json::Value& dic,
                                           unordered_map<string, const Json::Value&>& localScopeVars, StringList& visitedTemplates) const
    {
      string result;

      // iterate over all child in this branch.
      // the end is indicated by a child index of "invalid"
      size_t curIdx{idxFirstItem};
      while (curIdx != SyntaxTree::InvalidIndex)
      {
        const SyntaxTreeItem& sti = tree.at(curIdx);

        //
        // handler for static text
        //
        if (sti.t == SyntaxTreeItemType::Static)
        {
          result += sti.staticText;
        }

        //
        // handler for variables
        //
        if (sti.t == SyntaxTreeItemType::Variable)
        {
          // is the name of the form "first.second"?
          string first{};
          string second{};
          size_t idxDot = sti.varName.find('.');
          if (idxDot == string::npos)
          {
            first = sti.varName;
          } else {
            // split at the dot
            if (idxDot > 0) first = sti.varName.substr(0, idxDot);
            second = sti.varName.substr(idxDot + 1);
          }
          if (first.empty())
          {
            throw std::runtime_error("TemplateStore: invalid variable name: " + sti.varName);
          }

          // does "first" reference a "local variable"? if yes,
          // proceed with the local variable, otherwise pick the value
          // directly from dic
          auto it = localScopeVars.find(first);
          const Json::Value& var = (it != localScopeVars.end()) ? it->second : dic[first];

          // use "second" as a subscript, if existing
          if (second.empty())
          {
            result += var.asString();
          } else {
            result += var[second].asString();
          }
        }

        //
        // handler for include commands
        //
        if (sti.t == SyntaxTreeItemType::IncludeCmd)
        {
          result += getTemplate_Recursive(sti.varName, dic, visitedTemplates);
        }

        //
        // handler for if-conditions
        //
        if (sti.t == SyntaxTreeItemType::Condition)
        {
          // default: condition is NOT true
          bool cond = false;

          const Json::Value& val = dic[sti.varName];

          // only parse non-empty values. all empty values or non-existing variables
          // will be treated as "false" (see default)
          if (!(val.empty()))
          {
            // handle numeric values
            if (val.isConvertibleTo(Json::ValueType::booleanValue))
            {
              cond = val.asBool();
            } else {

              // this provides a simple interpreter for textual boolean expressions
              string s = val.asString();
              if (isInVector<string>({"yes", "true", "on", "YES", "TRUE", "ON"}, s))
              {
                cond = true;
              }

              // explicitly check for valid "false"-expressions, do not simply trust
              // the "default false" here
              if (!(isInVector<string>({"no", "false", "off", "NO", "FALSE", "OFF"}, s)))
              {
                throw std::runtime_error("TemplateStore: unparseable condition value: " + s);
              }
            }
          }

          if (sti.invertCondition) cond = !cond;

          // if the condition is true and the if-statement has actual
          // child items, make a recursive call to parse the subtree
          if (cond && (sti.idxFirstChild != SyntaxTree::InvalidIndex))
          {
            result += getSyntaxSubtree(tree, sti.idxFirstChild, dic, localScopeVars, visitedTemplates);
          }
        }

        //
        // handler for for-loops
        //
        if (sti.t == SyntaxTreeItemType::ForLoop)
        {
          // only process loops if there any items within the loop
          if (sti.idxFirstChild != SyntaxTree::InvalidIndex)
          {
            // does the list exist at all? And is it an array?
            const Json::Value& list = dic[sti.listName];
            if ((!(list.empty())) && (list.isArray()))
            {
              // iterate over the array contents
              for (int idx=0; idx < list.size(); ++idx)
              {
                const Json::Value& subList = list[idx];
                localScopeVars.emplace(sti.varName, subList);
                result += getSyntaxSubtree(tree, sti.idxFirstChild, dic, localScopeVars, visitedTemplates);
                localScopeVars.erase(sti.varName);
              }
            }
          }
        }

        // move on to the next sibling in this branch
        curIdx = sti.idxNextSibling;
      }

      return result;
    }

    //----------------------------------------------------------------------------



  }



  //----------------------------------------------------------------------------

}
