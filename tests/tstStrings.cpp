/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2021  Volker Knollmann
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
#include <limits>

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

TEST(Strings, ArgString)
{
  estring e{"abc % def %1 %a %% %2"};
  e.arg("X");
  ASSERT_EQ("abc % def X %a %% %2", e);

  e.arg("Y");
  ASSERT_EQ("abc % def X %a %% Y", e);

  e.arg("Z");
  ASSERT_EQ("abc % def X %a %% Y", e);

  //
  // corner cases
  //

  e = estring{""};  // empty source
  e.arg("X");
  ASSERT_EQ("", e);

  e = estring{"%42"};  // string is pure tag
  e.arg("Q");
  ASSERT_EQ("Q", e);

  e = estring{"%42"};  // string is pure tag
  e.arg("");  // empty replacement string
  ASSERT_TRUE(e.empty());

  e = estring{"%1%1"};
  e.arg("");  // empty replacement string
  ASSERT_TRUE(e.empty());

  e = estring{"%3%10%%2%1"};
  e.arg("%10");  // replace "%1" with "%10" ==> insert a new tag!
  e.arg("A");
  e.arg(42);
  e.arg("x");
  ASSERT_EQ("42x%Ax", e);
}

//----------------------------------------------------------------------------

TEST(Strings, ArgNumber)
{
  estring e{"abc % def %1 %a %% %2"};
  e.arg(-42);
  ASSERT_EQ("abc % def -42 %a %% %2", e);

  size_t st = SIZE_MAX;
  string s = to_string(st);
  e.arg(st);
  ASSERT_EQ("abc % def -42 %a %% " + s, e);
}

//----------------------------------------------------------------------------

TEST(Strings, ArgDouble)
{
  estring e{"abc %1 xyz %2 %4"};

  e.arg(3.14159, 2);
  ASSERT_EQ("abc 3.14 xyz %2 %4", e);

  e.arg(3.14159, 0);
  ASSERT_EQ("abc 3.14 xyz 3 %4", e);

  e.arg(3.14159, 8);
  ASSERT_EQ("abc 3.14 xyz 3 3.14159000", e);
}

//----------------------------------------------------------------------------

TEST(Strings, IsInt)
{
  ASSERT_TRUE(estring{"1"}.isInt());
  ASSERT_TRUE(estring{"42"}.isInt());
  ASSERT_TRUE(estring{"-4"}.isInt());
  ASSERT_FALSE(estring{""}.isInt());
  ASSERT_FALSE(estring{"fsdf"}.isInt());
  ASSERT_FALSE(estring{"-"}.isInt());
  ASSERT_FALSE(estring{"2.3"}.isInt());
  ASSERT_FALSE(estring{" "}.isInt());
  ASSERT_FALSE(estring{" 2"}.isInt());
  ASSERT_FALSE(estring{"2 "}.isInt());
  ASSERT_FALSE(estring{"2 4 5"}.isInt());
  ASSERT_FALSE(estring{"2.4"}.isInt());
}

//----------------------------------------------------------------------------

TEST(Strings, IsDouble)
{
  ASSERT_TRUE(estring{"1"}.isDouble());
  ASSERT_TRUE(estring{"42"}.isDouble());
  ASSERT_TRUE(estring{"-4"}.isDouble());

  ASSERT_TRUE(estring{"1."}.isDouble());
  ASSERT_TRUE(estring{"42."}.isDouble());
  ASSERT_TRUE(estring{"-4."}.isDouble());

  ASSERT_TRUE(estring{"1.01"}.isDouble());
  ASSERT_TRUE(estring{"42.01"}.isDouble());
  ASSERT_TRUE(estring{"-4.9"}.isDouble());

  ASSERT_TRUE(estring{".42"}.isDouble());
  ASSERT_TRUE(estring{"-.88"}.isDouble());

  ASSERT_FALSE(estring{""}.isDouble());
  ASSERT_FALSE(estring{"."}.isDouble());
  ASSERT_FALSE(estring{"-."}.isDouble());
  ASSERT_FALSE(estring{"-"}.isDouble());
  ASSERT_FALSE(estring{".-"}.isDouble());

  ASSERT_FALSE(estring{"fsdf"}.isDouble());
  ASSERT_FALSE(estring{"2.3."}.isDouble());
  ASSERT_FALSE(estring{" "}.isDouble());
  ASSERT_FALSE(estring{" 2.4.4"}.isDouble());
  ASSERT_FALSE(estring{"2 "}.isDouble());
  ASSERT_FALSE(estring{"2 4 5"}.isDouble());
  ASSERT_FALSE(estring{"-2.4 "}.isDouble());
}

//----------------------------------------------------------------------------

TEST(Strings, Split)
{
  estring e{"1, 2, 3"};
  vector<estring> v = e.split(",", true, true);
  ASSERT_EQ(3, v.size());
  ASSERT_EQ("1", v[0]);
  ASSERT_EQ("2", v[1]);
  ASSERT_EQ("3", v[2]);

  v = e.split(",", true, false);
  ASSERT_EQ(3, v.size());
  ASSERT_EQ("1", v[0]);
  ASSERT_EQ(" 2", v[1]);
  ASSERT_EQ(" 3", v[2]);

  e = estring{"1,,2"};
  v = e.split(",", true, true);
  ASSERT_EQ(3, v.size());
  ASSERT_EQ("1", v[0]);
  ASSERT_EQ("", v[1]);
  ASSERT_EQ("2", v[2]);
  v = e.split(",", false, true);
  ASSERT_EQ(2, v.size());
  ASSERT_EQ("1", v[0]);
  ASSERT_EQ("2", v[1]);

  e = estring{","};
  v = e.split(",", true, true);
  ASSERT_EQ(2, v.size());
  ASSERT_EQ("", v[0]);
  ASSERT_EQ("", v[1]);
  v = e.split(",", false, true);
  ASSERT_TRUE(v.empty());

  e = estring{"1,"};
  v = e.split(",", true, true);
  ASSERT_EQ(2, v.size());
  ASSERT_EQ("1", v[0]);
  ASSERT_EQ("", v[1]);
  v = e.split(",", false, true);
  ASSERT_EQ(1, v.size());
  ASSERT_EQ("1", v[0]);

  e = estring{""};
  v = e.split(",", true, true);
  ASSERT_TRUE(v.empty());

  e = estring{"abc"};
  v = e.split(",", true, true);
  ASSERT_EQ(1, v.size());
  ASSERT_EQ("abc", v[0]);
}

//----------------------------------------------------------------------------

