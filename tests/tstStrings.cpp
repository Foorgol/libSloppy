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
#include <thread>

#include <gtest/gtest.h>

#include "../Sloppy/String.h"

using namespace std;
using namespace Sloppy;

TEST(Strings, BasicCtor)
{
  // default ctor
  estring e;
  ASSERT_TRUE(e.empty());

  // from C-string
  const char* p = "abc123";
  estring e1{p};
  ASSERT_EQ("abc123", e1);
}

//----------------------------------------------------------------------------

TEST(Strings, CopyCtorAndAssignment)
{
  // copy ctor from another estring
  estring e1{"xyz"};
  estring e2{e1};
  ASSERT_EQ(e1, e2);
  e2.clear();
  ASSERT_TRUE(e1 != e2);  // proove that e1 was a copy, not a reference

  // copy ctor from std::string
  string s{"123abc"};
  estring e3{s};
  ASSERT_EQ(s, e3);

  // copy assignment from estring
  e2 = e1;
  ASSERT_EQ(e2, e1);
  e2.clear();
  ASSERT_TRUE(e1 != e2);  // proove that this was a copy, not a reference

  // copy assignment from std::string
  e2 = s;
  ASSERT_EQ(e2, s);
  e2.clear();
  ASSERT_TRUE(s != e2);  // proove that this was a copy, not a reference
}

//----------------------------------------------------------------------------

TEST(Strings, MoveCtorAndAssignment)
{
  // move ctor from another estring
  estring e1{"xyz"};
  estring e2{std::move(e1)};
  ASSERT_EQ("xyz", e2);
  ASSERT_TRUE(e1.empty());

  // move ctor from std::string
  string s{"123abc"};
  estring e3{std::move(s)};
  ASSERT_EQ("123abc", e3);
  ASSERT_TRUE(s.empty());

  // move assignment from estring
  e1 = std::move(e2);
  ASSERT_EQ("xyz", e1);
  ASSERT_TRUE(e2.empty());

  // move assignment from std::string
  s = "move_me";
  e2 = std::move(s);
  ASSERT_EQ("move_me", e2);
  ASSERT_TRUE(s.empty());
}

//----------------------------------------------------------------------------

TEST(Strings, Slice)
{
  estring e{"0123456789"};
  ASSERT_EQ("123", e.slice(1, 3));
  ASSERT_EQ("4", e.slice(4, 4));
  ASSERT_EQ("789", e.slice(7));
  ASSERT_EQ("", e.slice(70));
  ASSERT_THROW(e.slice(3,1), std::invalid_argument);
}

//----------------------------------------------------------------------------

TEST(Strings, Right)
{
  estring e{"0123456789"};
  ASSERT_EQ("789", e.right(3));
  ASSERT_EQ("", e.right(0));
  ASSERT_EQ("0123456789", e.right(100));
}

//----------------------------------------------------------------------------

TEST(Strings, Left)
{
  estring e{"0123456789"};
  ASSERT_EQ("012", e.left(3));
  ASSERT_EQ("", e.left(0));
  ASSERT_EQ("0123456789", e.left(100));
}

//----------------------------------------------------------------------------

TEST(Strings, ChopRight)
{
  estring e{"0123456789"};
  e.chopRight(0);
  ASSERT_EQ("0123456789", e);

  e.chopRight(2);
  ASSERT_EQ("01234567", e);

  // border case
  e.chopRight(e.size());
  ASSERT_TRUE(e.empty());

  e = estring{"abc"};
  e.chopRight(20);
  ASSERT_TRUE(e.empty());
}

//----------------------------------------------------------------------------

TEST(Strings, ChopLeft)
{
  estring e{"0123456789"};
  e.chopLeft(0);
  ASSERT_EQ("0123456789", e);

  e.chopLeft(2);
  ASSERT_EQ("23456789", e);

  // border case
  e.chopLeft(e.size());
  ASSERT_TRUE(e.empty());

  e = estring{"abc"};
  e.chopLeft(20);
  ASSERT_TRUE(e.empty());
}

//----------------------------------------------------------------------------

TEST(Strings, ChopRightCopy)
{
  estring e{"0123456789"};

  ASSERT_EQ("0123456789", e.chopRight_copy(0));

  ASSERT_EQ("01234567", e.chopRight_copy(2));

  ASSERT_TRUE(e.chopRight_copy(e.size()).empty());  // border case

  ASSERT_TRUE(e.chopRight_copy(20).empty());

  // the original string is still the same
  ASSERT_EQ("0123456789", e);
}

//----------------------------------------------------------------------------

TEST(Strings, ChopLeftCopy)
{
  estring e{"0123456789"};

  ASSERT_EQ("0123456789", e.chopLeft_copy(0));

  ASSERT_EQ("23456789", e.chopLeft_copy(2));

  ASSERT_TRUE(e.chopLeft_copy(e.size()).empty());  // border case

  ASSERT_TRUE(e.chopLeft_copy(20).empty());

  // the original string is still the same
  ASSERT_EQ("0123456789", e);
}

//----------------------------------------------------------------------------

TEST(Strings, StartsWith)
{
  estring e{"0123456789"};

  ASSERT_TRUE(e.startsWith("012"));
  ASSERT_FALSE(e.startsWith("ab"));
  ASSERT_FALSE(e.startsWith("0123456789sdjkfhsd"));
  ASSERT_TRUE(e.startsWith(""));

  estring e2{"01234"};
  ASSERT_TRUE(e.startsWith(e2));

  string s{"0"};
  ASSERT_TRUE(e.startsWith(s));
}

//----------------------------------------------------------------------------

TEST(Strings, EndsWith)
{
  estring e{"0123456789"};

  ASSERT_TRUE(e.endsWith("789"));
  ASSERT_FALSE(e.endsWith("ab"));
  ASSERT_FALSE(e.endsWith("0123456789sdjkfhsd"));
  ASSERT_TRUE(e.endsWith(""));

  estring e2{"89"};
  ASSERT_TRUE(e.endsWith(e2));

  string s{"9"};
  ASSERT_TRUE(e.endsWith(s));
}

//----------------------------------------------------------------------------

TEST(Strings, TrimLeft)
{
  estring e{"\t x "};
  e.trimLeft();
  ASSERT_EQ("x ", e);

  e = estring{""};
  e.trimLeft();
  ASSERT_TRUE(e.empty());

  e = estring{"    "};
  e.trimLeft();
  ASSERT_TRUE(e.empty());

  e = estring{"abc"};
  e.trimLeft();
  ASSERT_EQ("abc", e);
}

//----------------------------------------------------------------------------

TEST(Strings, TrimRight)
{
  estring e{" x \t"};
  e.trimRight();
  ASSERT_EQ(" x", e);

  e = estring{""};
  e.trimRight();
  ASSERT_TRUE(e.empty());

  e = estring{"    "};
  e.trimRight();
  ASSERT_TRUE(e.empty());

  e = estring{"abc"};
  e.trimRight();
  ASSERT_EQ("abc", e);
}

//----------------------------------------------------------------------------

TEST(Strings, Trim)
{
  estring e{" x \t"};
  e.trim();
  ASSERT_EQ("x", e);

  e = estring{""};
  e.trim();
  ASSERT_TRUE(e.empty());

  e = estring{"    "};
  e.trim();
  ASSERT_TRUE(e.empty());

  e = estring{"abc"};
  e.trim();
  ASSERT_EQ("abc", e);
}

//----------------------------------------------------------------------------

TEST(Strings, TrimCopy)
{
  estring e{" x \t"};
  estring t = e.trimLeft_copy();
  ASSERT_EQ("x \t", t);
  ASSERT_EQ(" x \t", e);

  t = e.trimRight_copy();
  ASSERT_EQ(" x", t);
  ASSERT_EQ(" x \t", e);

  t = e.trim_copy();
  ASSERT_EQ("x", t);
  ASSERT_EQ(" x \t", e);
}

//----------------------------------------------------------------------------

TEST(Strings, VectorCtor)
{
  vector<estring> parts{"abc", "def", "", "xy"};

  estring e{parts, ","};
  ASSERT_EQ("abc,def,,xy", e);

  e = estring{parts,""};
  ASSERT_EQ("abcdefxy", e);

  e = estring(vector<estring>{}, "");
  ASSERT_TRUE(e.empty());
}

//----------------------------------------------------------------------------

TEST(Strings, Contains)
{
  estring e{"0123456789"};

  ASSERT_TRUE(e.contains(""));
  ASSERT_TRUE(e.contains("45"));
  ASSERT_FALSE(e.contains("x"));
}

//----------------------------------------------------------------------------

TEST(Strings, Replace)
{
  estring empty;
  ASSERT_FALSE(empty.replaceFirst("sdf", "dkfj"));

  estring e{"0123456789"};
  ASSERT_FALSE(e.replaceFirst("sdf", "dkfj"));

  ASSERT_TRUE(e.replaceFirst("123", ""));
  ASSERT_EQ("0456789", e);

  ASSERT_TRUE(e.replaceFirst("89", "def"));
  ASSERT_EQ("04567def", e);

  ASSERT_TRUE(e.replaceFirst("0", "xy"));
  ASSERT_EQ("xy4567def", e);

  ASSERT_TRUE(e.replaceFirst("56", "QQ"));
  ASSERT_EQ("xy4QQ7def", e);

  ASSERT_FALSE(e.replaceFirst("", "AA"));
  ASSERT_EQ("xy4QQ7def", e);

  e = estring{"ab def ab xz "};
  ASSERT_FALSE(e.replaceAll("AAA", ""));
  ASSERT_EQ("ab def ab xz ", e);
  ASSERT_FALSE(e.replaceAll("AAA", "DDD"));
  ASSERT_EQ("ab def ab xz ", e);
  ASSERT_TRUE(e.replaceAll("ab", "DDD"));
  ASSERT_EQ("DDD def DDD xz ", e);
  ASSERT_FALSE(empty.replaceAll("sdf", "dkfj"));

  // special case where the replacement string
  // contains the pattern itself; note that we're
  // not ending up in an endless loop here!
  e = estring{"aaaaaaa"};
  ASSERT_TRUE(e.replaceAll("aa", "a"));
  ASSERT_EQ("aaaa", e);
  ASSERT_TRUE(e.replaceAll("a", "aa"));
  ASSERT_EQ("aaaaaaaa", e);
}

//----------------------------------------------------------------------------

TEST(Strings, ReplaceSection)
{
  estring empty;
  empty.replaceSection(10, 20, "abc");
  ASSERT_EQ("abc", empty);

  empty.clear();
  empty.replaceSection(0, 20, "abc");
  ASSERT_EQ("abc", empty);

  empty.clear();
  empty.replaceSection(0, 0, "abc");
  ASSERT_EQ("abc", empty);

  estring e{"0123456789"};
  e.replaceSection(0, 3, "XY");
  ASSERT_EQ("XY456789", e);
  e.replaceSection(4, 4, "_");
  ASSERT_EQ("XY45_789", e);
  e.replaceSection(100, 101, "::");
  ASSERT_EQ("XY45_789::", e);
  e.replaceSection(9, 9, "@");
  ASSERT_EQ("XY45_789:@", e);
  e.replaceSection(2, 4, "@");
  ASSERT_EQ("XY@789:@", e);
  e.replaceSection(0, 0, "");
  ASSERT_EQ("Y@789:@", e);
  e.replaceSection(6, 6, "");
  ASSERT_EQ("Y@789:", e);
  e.replaceSection(2, 2, "");
  ASSERT_EQ("Y@89:", e);
  e.replaceSection(0, e.size() - 1, "");
  ASSERT_TRUE(e.empty());

  ASSERT_THROW(e.replaceSection(1, 0, ""), std::invalid_argument);
  ASSERT_TRUE(e.empty());

  e = estring{"0123456789"};
  ASSERT_THROW(e.replaceSection(1, 0, ""), std::invalid_argument);
  ASSERT_EQ("0123456789", e);
  ASSERT_THROW(e.replaceSection(100, 99, ""), std::invalid_argument);
  ASSERT_EQ("0123456789", e);
  ASSERT_THROW(e.replaceSection(4, 3, "sfd"), std::invalid_argument);
  ASSERT_EQ("0123456789", e);

}

//----------------------------------------------------------------------------

TEST(Strings, ToUpperLower)
{
  estring e{"123abcöäüßáèô"};
  e.toUpper();
  ASSERT_EQ("123ABCÖÄÜßÁÈÔ", e);
  e.toLower();
  ASSERT_EQ("123abcöäüßáèô", e);
}

//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------


//----------------------------------------------------------------------------

