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

#include <iostream>

#include <gtest/gtest.h>

#include "../json.hpp"
#include "../Sloppy/TemplateProcessor/TemplateSys.h"
#include "BasicTestClass.h"

using namespace Sloppy::TemplateSystem;

class SyntaxTreeFixture : public BasicTestFixture
{
public:
  bool checkTreeItem(const SyntaxTreeItem& item, SyntaxTreeItemType t, size_t idxFirst = SyntaxTree::InvalidIndex, size_t idxLast = SyntaxTree::InvalidIndex)
  {
    if (item.t != t) return false;

    if (idxFirst != SyntaxTree::InvalidIndex)
    {
      if (item.idxFirstChar != idxFirst) return false;
    }

    if (idxLast != SyntaxTree::InvalidIndex)
    {
      if (item.idxLastChar != idxLast) return false;
    }

    return true;
  }

  bool checkTreeItem_Links(const SyntaxTreeItem& item, size_t idxParent, size_t idxNext, size_t idxChild = SyntaxTree::InvalidIndex)
  {
    if (item.idxParent != idxParent) return false;
    if (item.idxNextSibling != idxNext) return false;
    return (item.idxFirstChild == idxChild);
  }

  bool checkTreeItem_Var(const SyntaxTreeItem& item, const string& varName)
  {
    if (item.t != SyntaxTreeItemType::Variable) return false;

    return item.varName == varName;
  }

  bool checkTreeItem_Include(const SyntaxTreeItem& item, const string& fName)
  {
    if (item.t != SyntaxTreeItemType::IncludeCmd) return false;

    return (item.varName == fName);
  }

  bool checkTreeItem_if(const SyntaxTreeItem& item, const string& varName, bool isInverted)
  {
    if (item.t != SyntaxTreeItemType::Condition) return false;

    if (item.varName != varName) return false;
    return (item.invertCondition == isInverted);
  }

  bool checkTreeItem_for(const SyntaxTreeItem& item, const string& varName, const string& listName)
  {
    if (item.t != SyntaxTreeItemType::ForLoop) return false;

    if (item.varName != varName) return false;
    return (item.listName == listName);
  }

  bool checkTreeItem_static(const SyntaxTreeItem& item, const string& doc, const string& itemContent)
  {
    if (item.t != SyntaxTreeItemType::Static) return false;
    size_t len = item.idxLastChar - item.idxFirstChar + 1;
    string s = doc.substr(item.idxFirstChar, len);
    return (s == itemContent);
  }

  bool parse_checkError_compareCount(const string& doc, vector<SyntaxTreeItem>& tree, SyntaxTreeError& err, size_t cnt, bool expectError)
  {
    SyntaxTree st;
    err = st.parse(doc);
    if (expectError != err.isError()) return false;

    // create a deep copy of the tree items
    //auto tmpTree = st.getTree();
    const auto& tmpTree = st.getTree();
    if (tmpTree.size() != cnt) return false;
    tree.clear();
    std::copy(tmpTree.cbegin(), tmpTree.cend(), back_inserter(tree));

    return true;
  }
};

//----------------------------------------------------------------------------

TEST(BetterTemplateSys, ctor)
{
  // this should run without any problems
  TemplateStore ts{"../tests/sampleTemplateStore", {"txt", "html"}};
  ts = TemplateStore{"../tests/sampleTemplateStore/", {"txt", "html"}};

  // point to invalid directory
  ASSERT_THROW(TemplateStore("/blabla", {"txt", "html"}), std::invalid_argument);

  // point to file instead of directory
  ASSERT_THROW(TemplateStore("../tests/sampleTemplateStore/t1.txt", {"txt", "html"}), std::invalid_argument);

  // filter out all files
  ASSERT_THROW(TemplateStore("../tests/sampleTemplateStore", {"xyz",}), std::invalid_argument);
}

//----------------------------------------------------------------------------

TEST_F(SyntaxTreeFixture, SyntaxTree_Basic_If)
{
  constexpr size_t noLink = SyntaxTree::InvalidIndex;

  // empty document
  string s;
  s.clear();
  vector<SyntaxTreeItem> tree;
  SyntaxTreeError err;
  ASSERT_TRUE(parse_checkError_compareCount(s, tree, err, 0, false));

  // simple document without any tokens
  s = "a";
  ASSERT_TRUE(parse_checkError_compareCount(s, tree, err, 1, false));
  SyntaxTreeItem i = tree[0];
  ASSERT_TRUE(checkTreeItem_static(i, s, "a"));
  ASSERT_TRUE(checkTreeItem_Links(i, noLink, noLink, noLink));

  // a simple document that consists of only a variable
  s = "{{a}}";
  ASSERT_TRUE(parse_checkError_compareCount(s, tree, err, 1, false));
  i = tree[0];
  ASSERT_TRUE(checkTreeItem_Var(i, "a"));
  ASSERT_TRUE(checkTreeItem_Links(i, noLink, noLink, noLink));

  // a valid if-statement without content
  s = "{{if var}}{{endif}}";
  ASSERT_TRUE(parse_checkError_compareCount(s, tree, err, 1, false));
  i = tree[0];
  ASSERT_TRUE(checkTreeItem_if(i, "var", false));
  ASSERT_TRUE(checkTreeItem_Links(i, noLink, noLink, noLink));

  // a correct if-statement with inner content and sourrounding content
  s = "a{{if !var}}b{{endif}}c";
  ASSERT_TRUE(parse_checkError_compareCount(s, tree, err, 4, false));

  i = tree[0];
  ASSERT_TRUE(checkTreeItem_static(i, s, "a"));
  ASSERT_TRUE(checkTreeItem_Links(i, noLink, 1, noLink));

  i = tree[1];
  ASSERT_TRUE(checkTreeItem_if(i, "var", true));
  ASSERT_TRUE(checkTreeItem_Links(i, noLink, 3, 2));

  i = tree[2];
  ASSERT_TRUE(checkTreeItem_static(i, s, "b"));
  ASSERT_TRUE(checkTreeItem_Links(i, 1, noLink, noLink));

  i = tree[3];
  ASSERT_TRUE(checkTreeItem_static(i, s, "c"));
  ASSERT_TRUE(checkTreeItem_Links(i, noLink, noLink, noLink));

  // nested ifs
  s = "a{{if !var}}b{{if foo}}xy{{endif}}{{endif}}c";
  ASSERT_TRUE(parse_checkError_compareCount(s, tree, err, 6, false));

  i = tree[0];
  ASSERT_TRUE(checkTreeItem_static(i, s, "a"));
  ASSERT_TRUE(checkTreeItem_Links(i, noLink, 1, noLink));

  i = tree[1];
  ASSERT_TRUE(checkTreeItem_if(i, "var", true));
  ASSERT_TRUE(checkTreeItem_Links(i, noLink, 5, 2));

  i = tree[2];
  ASSERT_TRUE(checkTreeItem_static(i, s, "b"));
  ASSERT_TRUE(checkTreeItem_Links(i, 1, 3, noLink));

  i = tree[3];
  ASSERT_TRUE(checkTreeItem_if(i, "foo", false));
  ASSERT_TRUE(checkTreeItem_Links(i, 1, noLink, 4));

  i = tree[4];
  ASSERT_TRUE(checkTreeItem_static(i, s, "xy"));
  ASSERT_TRUE(checkTreeItem_Links(i, 3, noLink, noLink));

  i = tree[5];
  ASSERT_TRUE(checkTreeItem_static(i, s, "c"));
  ASSERT_TRUE(checkTreeItem_Links(i, noLink, noLink, noLink));
}

//----------------------------------------------------------------------------

TEST_F(SyntaxTreeFixture, SyntaxTree_For)
{
  constexpr size_t noLink = SyntaxTree::InvalidIndex;

  // an empty for-loop
  string s = "{{ for var : list }}{{endfor}}";
  vector<SyntaxTreeItem> tree;
  SyntaxTreeError err;
  ASSERT_TRUE(parse_checkError_compareCount(s, tree, err, 1, false));

  SyntaxTreeItem i = tree[0];
  ASSERT_TRUE(checkTreeItem_for(i, "var", "list"));
  ASSERT_TRUE(checkTreeItem_Links(i, noLink, noLink, noLink));

  // a nested for-if-text
  s = "a {{ for var : list }} bc {{ if cond }} de {{endif}} fg {{endfor}} hi";
  ASSERT_TRUE(parse_checkError_compareCount(s, tree, err, 7, false));

  i = tree[0];
  ASSERT_TRUE(checkTreeItem_static(i, s, "a "));
  ASSERT_TRUE(checkTreeItem_Links(i, noLink, 1, noLink));

  i = tree[1];
  ASSERT_TRUE(checkTreeItem_for(i, "var", "list"));
  ASSERT_TRUE(checkTreeItem_Links(i, noLink, 6, 2));

  i = tree[2];
  ASSERT_TRUE(checkTreeItem_static(i, s, " bc "));
  ASSERT_TRUE(checkTreeItem_Links(i, 1, 3, noLink));

  i = tree[3];
  ASSERT_TRUE(checkTreeItem_if(i, "cond", false));
  ASSERT_TRUE(checkTreeItem_Links(i, 1, 5, 4));

  i = tree[4];
  ASSERT_TRUE(checkTreeItem_static(i, s, " de "));
  ASSERT_TRUE(checkTreeItem_Links(i, 3, noLink, noLink));

  i = tree[5];
  ASSERT_TRUE(checkTreeItem_static(i, s, " fg "));
  ASSERT_TRUE(checkTreeItem_Links(i, 1, noLink, noLink));

  i = tree[6];
  ASSERT_TRUE(checkTreeItem_static(i, s, " hi"));
  ASSERT_TRUE(checkTreeItem_Links(i, noLink, noLink, noLink));
}

//----------------------------------------------------------------------------

TEST_F(SyntaxTreeFixture, SyntaxTree_Include)
{
  constexpr size_t noLink = SyntaxTree::InvalidIndex;

  // a simple include
  string s = "{{ include otherFile.txt }}";
  vector<SyntaxTreeItem> tree;
  SyntaxTreeError err;
  ASSERT_TRUE(parse_checkError_compareCount(s, tree, err, 1, false));

  SyntaxTreeItem i = tree[0];
  ASSERT_TRUE(checkTreeItem_Include(i, "otherFile.txt"));
}

//----------------------------------------------------------------------------

TEST(BetterTemplateSys, SimpleGet)
{
  TemplateStore ts{"../tests/sampleTemplateStore", {"txt", "html"}};

  // prepare a json structure with target values
  json val;
  val["x"] = "***X***";
  val["y"] = 42;

  string s = ts.get("t1.txt", val);
  string sExpected = "Hello\nThis is a variable: ***X*** and this as well 42.";
  sExpected += "\n\n***included***\n\n\n";

  ASSERT_EQ(sExpected, s);
}

//----------------------------------------------------------------------------

TEST(BetterTemplateSys, GetWithIf)
{
  TemplateStore ts{"../tests/sampleTemplateStore", {"txt", "html"}};
  string sFalse = "Intro\nOutro\n";
  string sTrue = "Intro\nConditionalText\nOutro\n";

  // prepare a json structure with target values
  // start with an empty list
  json val;

  string s = ts.get("ifTest.txt", val);
  ASSERT_EQ(sFalse, s);

  val["condVar"] = 1;
  s = ts.get("ifTest.txt", val);
  ASSERT_EQ(sTrue, s);
  val["condVar"] = true;
  s = ts.get("ifTest.txt", val);
  ASSERT_EQ(sTrue, s);


  val["condVar"] = 0;
  s = ts.get("ifTest.txt", val);
  ASSERT_EQ(sFalse, s);
  val["condVar"] = false;
  s = ts.get("ifTest.txt", val);
  ASSERT_EQ(sFalse, s);

  for (const string& v : {"yes", "true", "on", "YES", "TRUE", "ON", "Yes", "True", "On", "1"})
  {
    val["condVar"] = v;
    s = ts.get("ifTest.txt", val);
    ASSERT_EQ(sTrue, s);
  }

  for (const string& v : {"no", "false", "off", "No", "False", "Off", "NO", "FALSE", "OFF", "0"})
  {
    val["condVar"] = v;
    s = ts.get("ifTest.txt", val);
    ASSERT_EQ(sFalse, s);
  }
}

//----------------------------------------------------------------------------

TEST(BetterTemplateSys, RecursiveInclude_MultiInclude)
{
  TemplateStore ts{"../tests/sampleTemplateStore", {"txt", "html"}};

  json val;
  ASSERT_THROW(ts.get("recursion1.txt", val), std::runtime_error);
  ASSERT_THROW(ts.get("recursion2a.txt", val), std::runtime_error);

  string s = ts.get("multiInclude.txt", val);
  string sExpected = "***included***\n\n";
  sExpected += sExpected;
  ASSERT_EQ(sExpected, s);
}

//----------------------------------------------------------------------------

TEST(BetterTemplateSys, Subkeys)
{
  TemplateStore ts{"../tests/sampleTemplateStore", {"txt", "html"}};

  json val;
  val["normal"] = "abc123";
  val["one"]["two"] = 2;

  string s = ts.get("subkeys.txt", val);
  string sExpected = "abc123\n2\n\n\n";
  ASSERT_EQ(sExpected, s);
}

//----------------------------------------------------------------------------

TEST(BetterTemplateSys, Loops)
{
  TemplateStore ts{"../tests/sampleTemplateStore", {"txt", "html"}};

  json dic;
  json li = json::array();
  li.push_back("one");
  li.push_back("two");
  li.push_back("three");
  dic["list1"] = li;

  li = json::array();
  for (int i=0; i < 3 ; ++i)
  {
    json dataSet;
    dataSet["key"] =  "k" + to_string(i);
    dataSet["val"] =  "v" + to_string(i);
    li.push_back(dataSet);
  }
  dic["list2"] = li;

  string s = ts.get("forTest.txt", dic);
  string sExpected = "header\n\nxy\n\n  * one\n";
  sExpected += "  * two\n  * three\n\n\n  * k0 ==> v0\n";
  sExpected += "  * k1 ==> v1\n  * k2 ==> v2\nfooter\n";
  ASSERT_EQ(sExpected, s);

}

//----------------------------------------------------------------------------

TEST(BetterTemplateSys, StringList)
{
  TemplateStore ts{"../tests/sampleTemplateStore", {"txt", "html"}};

  ASSERT_TRUE(ts.setStringlist("../tests/sampleTemplateStore/stringlist.lst"));

  // get a few strings directly from the template store
  optional<string> s = ts.getString("s1");
  ASSERT_TRUE(s.has_value());
  ASSERT_EQ("some string", *s);

  s = ts.getString("s1", "de");
  ASSERT_TRUE(s.has_value());
  ASSERT_EQ("eine Zeichenkette", *s);

  s = ts.getString("sfkjsdf", "de");
  ASSERT_FALSE(s.has_value());

  s = "nonempty";
  s = ts.getString("lkjo");
  ASSERT_FALSE(s.has_value());

  json dic;
  s = ts.get("stringlist.txt", dic);
  ASSERT_EQ("some other string\n", s);
}

//----------------------------------------------------------------------------

TEST(BetterTemplateSys, NestedFor)
{
  TemplateStore ts{"../tests/sampleTemplateStore", {"txt", "html"}};

  json dic;
  json outer = json::array();

  json inner;
  inner["major"] = "a";
  json sub = json::array();
  sub[0] = 0;
  sub[1] = 1;
  sub[2] = 2;
  inner["subs"] = sub;
  outer.push_back(inner);

  inner = json();
  inner["major"] = "b";
  sub = json::array();
  sub[0] = 3;
  sub[1] = 4;
  sub[2] = 5;
  inner["subs"] = sub;
  outer.push_back(inner);

  dic["list1"] = outer;
  //cout << dic.toStyledString() << endl;

  string s = ts.get("nestedFor.txt", dic);
  cout << s;
  string sExpected = "header\n  * Major value: a\n  * Sub-values:\n    - 0\n    - 1\n    - 2\n";
  sExpected += "  * Major value: b\n  * Sub-values:\n    - 3\n    - 4\n    - 5\n";
  sExpected += "footer\n";
  ASSERT_EQ(sExpected, s);

}
