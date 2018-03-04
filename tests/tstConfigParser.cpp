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

#include "../Sloppy/ConfigFileParser/ConfigFileParser.h"
#include "BasicTestClass.h"

using namespace Sloppy;

TEST(ConfigParser, Basics)
{
  // create a dummy config file
  ostringstream s;
  s << "var1 = 1" << endl;
  s << "var 2=ab cd/ef\\g" << endl;
  s << " var1 = 42" << endl;
  s << "  # comment" << endl;
  s << endl;
  s << "\tVar3 =" << endl;
  s << " 3 ==4=5" << endl;
  s << " boolCheck0 = true" << endl;
  s << " boolCheck1 = 1" << endl;
  s << " boolCheck2 = On" << endl;
  s << " boolCheck3 = YeS" << endl;
  s << " boolCheck4 = fALse" << endl;
  s << " boolCheck5 = 0" << endl;
  s << " boolCheck6 = NO" << endl;
  s << " boolCheck7 = oFF" << endl;
  s << "var3 = x";  // no endl here to test that the last line is parsed as well!

  // parse it
  istringstream is{s.str()};
  Parser cp(is);

  // check it
  ASSERT_TRUE(cp.hasKey("var1"));
  ASSERT_EQ("42", cp.getValue("var1"));  // "1" should have been overwritten!
  ASSERT_EQ("x", cp.getValue("var3"));
  ASSERT_EQ("", cp.getValue("Var3"));
  ASSERT_EQ("ab cd/ef\\g", cp.getValue("var 2"));
  ASSERT_EQ("=4=5", cp.getValue("3"));

  // test conversion to int
  optional<int> oi = cp.getValueAsInt("var1");
  ASSERT_TRUE(oi.has_value());
  ASSERT_EQ(42, oi);
  ASSERT_FALSE(cp.getValueAsInt("var3").has_value());  // conversion of a 'x'
  ASSERT_FALSE(cp.getValueAsInt("Var3").has_value());  // conversion of an empty string

  // test conversion to bool
  for (int i=0 ; i < 4; ++i)
  {
    string keyName = "boolCheck" + to_string(i);
    optional<bool> ob = cp.getValueAsBool(keyName);
    ASSERT_TRUE(ob.has_value());
    ASSERT_TRUE(ob.value());
  }
  for (int i=4 ; i < 8; ++i)
  {
    string keyName = "boolCheck" + to_string(i);
    optional<bool> ob = cp.getValueAsBool(keyName);
    ASSERT_TRUE(ob.has_value());
    ASSERT_FALSE(ob.value());
  }
  optional<bool> ob = cp.getValueAsBool("var3"); // conversion of a 'x'
  ASSERT_FALSE(ob.has_value());
  ob = cp.getValueAsBool("Var3"); // conversion of an empty string
  ASSERT_FALSE(ob.has_value());
}

//----------------------------------------------------------------------------

TEST(ConfigParser, Sections)
{
  // create a dummy config file
  ostringstream s;
  s << "var1 = 0" << endl;
  s << "  [Sec1]  " << endl;
  s << "var1 = 1" << endl;
  s << "[Sec2] some [Garb]age " << endl;
  s << "var1 = 2" << endl;
  s << "[Sec3]" << endl;
  s << "[  Sec with spaces  ]" << endl;
  s << "[Sec1]" << endl;
  s << "var2 = 88" << endl;

  // parse it
  istringstream is{s.str()};
  Parser cp(is);

  // check it
  ASSERT_TRUE(cp.hasKey("var1"));
  ASSERT_EQ("0", cp.getValue("var1"));
  ASSERT_EQ("1", cp.getValue("Sec1", "var1"));
  ASSERT_EQ("88", cp.getValue("Sec1", "var2"));
  ASSERT_EQ("2", cp.getValue("Sec2", "var1"));
  ASSERT_TRUE(cp.hasSection("Sec3"));
  ASSERT_TRUE(cp.hasSection("Sec with spaces"));
  ASSERT_FALSE(cp.hasSection("Garb"));
}
