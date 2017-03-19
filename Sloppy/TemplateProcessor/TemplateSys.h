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

#ifndef SLOPPY__TEMPLATE_SYSTEM_H
#define SLOPPY__TEMPLATE_SYSTEM_H

#include <istream>
#include <unordered_map>
#include <memory>
#include <vector>
#include <regex>

#include "../libSloppy.h"

using namespace std;

namespace Sloppy
{
  namespace TemplateSystem
  {
    enum class SyntaxTreeItemType
    {
      Static,
      Variable,
      ForLoop,
      Condition,
      IncludeCmd
    };

    enum class TokenType
    {
      Variable,
      StartIf,
      EndIf,
      StartFor,
      EndFor,
      IncludeCmd
    };

    enum class SyntaxSectionType
    {
      Root,
      ForLoop,
      Condition
    };

    struct SyntaxTreeItem
    {
      SyntaxTreeItemType t;
      string varName;
      string listName;
      bool invertCondition;

      size_t idxNextSibling;
      size_t idxFirstChild;
      size_t idxParent;   // is redundant, but makes traversal in both direction easier

      size_t idxFirstChar;
      size_t idxLastChar;
    };

    struct SyntaxTreeError
    {
      size_t line;
      size_t idxFirstChar;
      size_t idxLastChar;
      string msg;

      SyntaxTreeError(const sregex_iterator::value_type& tokenMatch, const string& _msg)
        :SyntaxTreeError{_msg}
      {
        updatePosition(tokenMatch);
      }

      SyntaxTreeError()
        :idxFirstChar{0}, idxLastChar{0}, msg{} {}

      explicit SyntaxTreeError(const string& _msg)
        :idxFirstChar{0}, idxLastChar{0}, msg{_msg} {}

      bool isError() { return (!(msg.empty())); }

      void updatePosition(const sregex_iterator::value_type& tokenMatch)
      {
        idxFirstChar = tokenMatch.position();
        idxLastChar = idxFirstChar + tokenMatch.size() - 1;
      }

      string str()
      {
        string s = "Template parsing error at position %1: ";
        strArg(s, (int) idxFirstChar);
        s += msg;
        return s;
      }
    };

    class SyntaxTree
    {
    public:
      static constexpr size_t InvalidIndex = (1 << 31);
      explicit SyntaxTree();
      SyntaxTreeError parse(const string& s);
      vector<SyntaxTreeItem> getTree() const { return tree; }

    private:
      vector<SyntaxTreeItem> tree;

      // store a few regex objects in the class although we need them only
      // in the recursive syntax parser. This avoids the expensive re-instantiation
      // of the regexs each time we enter the recursion loop
      regex reToken;
      regex reFor;
      regex reIf;
      regex reVar;
      regex reInclude;

      tuple<TokenType, bool> checkToken(const string& token) const;

      tuple<TokenType, SyntaxTreeError> doSyntaxCheck(const string& token, SyntaxSectionType secType) const;
      int isValid_endif(const string& token) const;
      int isValid_if(const string& token) const;
      int isValid_endfor(const string& token) const;
      int isValid_for(const string& token) const;
      int isValid_include(const string& token) const;
      bool isValid_var(const string& token) const;
    };

    //----------------------------------------------------------------------------

    class Template
    {
    public:
      explicit Template(istream& inData);
      explicit Template(const string& inData);
      static unique_ptr<Template> fromFile(const string& fName);

      SyntaxTreeError parse();
      bool isSyntaxOkay() { return syntaxOkay; }

    protected:
      string rawData;
      SyntaxTree st;
      bool syntaxOkay;
    };

    //----------------------------------------------------------------------------

    class TemplateStore
    {
    public:
      explicit TemplateStore(const string& rootDir, const StringList& extList);

    private:
      unordered_map<string, Template> docs;
    };
  }
}

#endif
