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
#include <sstream>

#include <gtest/gtest.h>

#include "../Sloppy/TemplateProcessor/Template.h"
#include "../Sloppy/TemplateProcessor/TemplateCollection.h"
#include "BasicTestClass.h"

using namespace Sloppy::TemplateProcessor;

TEST(TemplateProcessor, TemplateFunc)
{
  // create a dummy template
  ostringstream ss;
  ss << "abc ##var1# def ##var2#";

  // parse it
  Template t{ss.str()};

  // check it
  SubstDic d{{"var1", "1"}, {"var2", "2"}};
  string out;
  t.getSubstitutedData(out, d, "##", "#");
  ASSERT_EQ("abc 1 def 2", out);

  // check keys that are part of each other
  string s = "123 a_bc_de 123 bc 123";
  t = Template{s};
  d = SubstDic{{"bc", "x"}, {"a_bc_de", "y"}};
  t.getSubstitutedData(out, d);
  ASSERT_EQ("123 y 123 x 123", out);
  d = SubstDic{{"a_bc_de", "y"}, {"bc", "x"}};  // test internal sorting
  t.getSubstitutedData(out, d);
  ASSERT_EQ("123 y 123 x 123", out);
}

//----------------------------------------------------------------------------

TEST(TemplateProcessor, TemplateCollectionFunc)
{
  // create a dummy template collection
  TemplateCollection tc;
  ASSERT_TRUE(tc.addTemplate("t1", "T1: abc var1 def"));
  ASSERT_TRUE(tc.addTemplate("t2", "T2: abc var1 def"));
  ASSERT_FALSE(tc.addTemplate("t2", "lalala"));

  // test substitutions
  SubstDic d{{"var1", "1"}, {"var2", "2"}};
  string out;
  ASSERT_TRUE(tc.getSubstitutedData("t1", out, d));
  ASSERT_EQ("T1: abc 1 def", out);
  ASSERT_TRUE(tc.getSubstitutedData("t2", out, d));
  ASSERT_EQ("T2: abc 1 def", out);
  ASSERT_FALSE(tc.getSubstitutedData("sfdkjsdkf", out, d));

  // test erase
  ASSERT_FALSE(tc.removeTemplate("skdf"));
  ASSERT_TRUE(tc.removeTemplate("t2"));
  ASSERT_FALSE(tc.getSubstitutedData("t2", out, d));
}
