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

//----------------------------------------------------------------------------

bool constraintTestHelper(const string& rawInput, ValueConstraint c, int idxFailMax, int idxValidMax)
{
  // parse the string
  istringstream is{rawInput};
  Parser cp(is);

  // check the constraints
  for (int i = 0; i <= idxFailMax; ++i)
  {
    string k = "fail" + to_string(i);
    if (cp.checkConstraint(k, c))
    {
      cerr << "Parser constraint check: key " << k << " should fail the check but passed!";
      return false;
    };
  }
  for (int i = 0; i <= idxValidMax; ++i)
  {
    string k = "valid" + to_string(i);
    if (!(cp.checkConstraint(k, c)))
    {
      cerr << "Parser constraint check: key " << k << " should pass the check but failed!";
      return false;
    };
  }

  return true;
}
//----------------------------------------------------------------------------

TEST(ConfigParser, Constraints_NotEmpty)
{
  string s = R"(
      empty =
      valid = x
             )";

  // parse the string
  istringstream is{s};
  Parser cp(is);

  // check the constraints
  ASSERT_FALSE(cp.checkConstraint("nonexisting", ValueConstraint::NotEmpty));
  ASSERT_FALSE(cp.checkConstraint("empty", ValueConstraint::NotEmpty));
  ASSERT_TRUE(cp.checkConstraint("valid", ValueConstraint::NotEmpty));
}

//----------------------------------------------------------------------------

TEST(ConfigParser, Constraints_Alnum)
{
  string s = R"(
      fail0 = _
      fail1 = abc,123
      fail2 = ö
      fail3 =
      fail4 = -2
      valid0 = x
      valid1 = 2
      valid2 = x42
             )";

  ASSERT_TRUE(constraintTestHelper(s, ValueConstraint::Alnum, 4, 2));
}

//----------------------------------------------------------------------------

TEST(ConfigParser, Constraints_Alpha)
{
  string s = R"(
      fail0 = _
      fail1 = abc,123
      fail2 = ö
      fail3 =
      fail4 = -2
      fail5 = 2
      fail6 = x42
      valid0 = x
      valid1 = xsfddf
             )";

  ASSERT_TRUE(constraintTestHelper(s, ValueConstraint::Alpha, 6, 1));
}

//----------------------------------------------------------------------------

TEST(ConfigParser, Constraints_Digit)
{
  string s = R"(
      fail0 = _
      fail1 = abc,123
      fail2 = ö
      fail3 =
      fail4 = -2
      fail5 = x
      valid0 = 0
      valid1 = 42
             )";

  ASSERT_TRUE(constraintTestHelper(s, ValueConstraint::Digit, 5, 1));
}

//----------------------------------------------------------------------------

TEST(ConfigParser, Constraints_Numeric)
{
  string s = R"(
      fail0 = _
      fail1 = abc,123
      fail2 = ö
      fail3 =
      fail4 = x
      fail5 = 12.45.6
      fail6 = 1,2
      fail7 = 44a
      fail8 = -4.5b
      valid0 = 0
      valid1 = 42
      valid2 = -2
      valid3 = 1.546
      valid4 = -2.456
             )";

  ASSERT_TRUE(constraintTestHelper(s, ValueConstraint::Numeric, 8, 4));
}

//----------------------------------------------------------------------------

TEST(ConfigParser, Constraints_Integer)
{
  string s = R"(
      fail0 = _
      fail1 = abc,123
      fail2 = ö
      fail3 =
      fail4 = x
      fail5 = 12.45.6
      fail6 = 1,2
      fail7 = 44a
      fail8 = -4.5b
      fail9 = 1.546
      fail10 = -2.456
      valid0 = 0
      valid1 = 42
      valid2 = -2
             )";

  ASSERT_TRUE(constraintTestHelper(s, ValueConstraint::Integer, 10, 2));
}

//----------------------------------------------------------------------------

TEST(ConfigParser, Constraints_Bool)
{
  string s = R"(
      fail0 = _
      fail1 = abc,123
      fail2 = ö
      fail3 =
      fail4 = x
      fail5 = 12.45.6
      fail6 = 1,2
      fail7 = 44a
      fail8 = -4.5b
      fail9 = 1.546
      fail10 = -2.456
      fail11 = 42
      fail12 = -2
      valid0 = 0
      valid1 = 1
      valid2 = on
      valid3 = oFf
      valid4 = TRue
      valid5 = faLSe
             )";

  ASSERT_TRUE(constraintTestHelper(s, ValueConstraint::Bool, 12, 5));
}

//----------------------------------------------------------------------------

TEST(ConfigParser, Constraints_File)
{
  string s = R"(
      fail0 =
      fail1 = /usr
      fail2 = /dev/sda
      fail3 = kfdjhsdkf
      valid0 = /usr/bin/bash
      valid1 = ./Sloppy_Tests
             )";

  ASSERT_TRUE(constraintTestHelper(s, ValueConstraint::File, 3, 1));
}

//----------------------------------------------------------------------------

TEST(ConfigParser, Constraints_Dir)
{
  string s = R"(
      fail0 =
      fail1 = /usr/bin/bash
      fail2 = ./Sloppy_Tests
      fail3 = /dev/sda
      fail4 = kfdjhsdkf
      valid0 = .
      valid1 = ..
      valid2 = ../debug
      valid3 = /usr
             )";

  ASSERT_TRUE(constraintTestHelper(s, ValueConstraint::Directory, 4, 3));
}

//----------------------------------------------------------------------------

TEST(ConfigParser, Constraints_Timezone)
{
  string s = R"(
      fail0 =
      fail1 = dlkjgdfg
      fail2 = Europe/Bla
      valid0 = Europe/Berlin
             )";

  ASSERT_TRUE(constraintTestHelper(s, ValueConstraint::StandardTimezone, 2, 0));
}

//----------------------------------------------------------------------------

TEST(ConfigParser, Constraints_IsoDate)
{
  string s = R"(
      fail0 =
      fail1 = dlkjgdfg
      fail2 = 1234-vb-32
      fail3 = 2000-0-0
      fail4 = 2000-13-12
      fail5 = 2001-2-29
      fail6 = 2018-7-32
      valid0 = 2018-7-15
      valid1 = 2016-02-29
             )";

  ASSERT_TRUE(constraintTestHelper(s, ValueConstraint::IsoDate, 6, 1));
}

//----------------------------------------------------------------------------

TEST(ConfigParser, Constraints_Bulk)
{
  string s = R"(
      [MySection]
      k1 = ssldfjsljf
      k2 = 43234
      k3 = -3.14
      k4 = off
      k5 = /usr/bin
      k6 = 2018-07-15
             )";

  istringstream is{s};
  Parser cp(is);

  bool isOkay = cp.bulkCheckConstraints(
  {{"MySection", "k1", ValueConstraint::NotEmpty},
   {"MySection", "k2", ValueConstraint::Digit},
   {"MySection", "k3", ValueConstraint::Numeric},
   {"MySection", "k4", ValueConstraint::Bool},
   {"MySection", "k5", ValueConstraint::Directory},
   {"MySection", "k6", ValueConstraint::IsoDate},
        });
  ASSERT_TRUE(isOkay);

  isOkay = cp.bulkCheckConstraints(
  {{"MySection", "k1", ValueConstraint::NotEmpty},
   {"MySection", "k2", ValueConstraint::Digit},
   {"MySection", "k3", ValueConstraint::Numeric},
   {"MySection", "k4", ValueConstraint::Bool},
   {"MySection", "k5", ValueConstraint::File},
   {"MySection", "k6", ValueConstraint::IsoDate},
        });
  ASSERT_FALSE(isOkay);
}

//----------------------------------------------------------------------------

TEST(ConfigParser, Constraints_IntRange)
{
  string s = R"(
      [MySection]
      k1 = -3
      k2 = 0
      k3 = 42
             )";

  istringstream is{s};
  Parser cp(is);
  ASSERT_TRUE(cp.checkConstraint_IntRange("MySection", "k1", {}, 5));
  ASSERT_TRUE(cp.checkConstraint_IntRange("MySection", "k1", -4, 5));
  ASSERT_TRUE(cp.checkConstraint_IntRange("MySection", "k1", -4, {}));
  ASSERT_TRUE(cp.checkConstraint_IntRange("MySection", "k2", 0, 0));

  ASSERT_FALSE(cp.checkConstraint_IntRange("MySection", "k2", 1, {}));
  ASSERT_FALSE(cp.checkConstraint_IntRange("MySection", "k2", {}, -1));

  ASSERT_THROW(cp.checkConstraint_IntRange("MySection", "k2", 11, 10), std::range_error);
}

//----------------------------------------------------------------------------

TEST(ConfigParser, Constraints_StrLen)
{
  string s = R"(
      k = abc
             )";

  istringstream is{s};
  Parser cp(is);

  ASSERT_TRUE(cp.checkConstraint_StrLen("k", {}, 5));
  ASSERT_TRUE(cp.checkConstraint_StrLen("k", 0, {}));
  ASSERT_TRUE(cp.checkConstraint_StrLen("k", 2, {}));
  ASSERT_TRUE(cp.checkConstraint_StrLen("k", 1, 5));

  ASSERT_FALSE(cp.checkConstraint_StrLen("k", 4, {}));
  ASSERT_FALSE(cp.checkConstraint_StrLen("k", {}, 2));

  ASSERT_THROW(cp.checkConstraint_StrLen("k", 11, 10), std::range_error);
}

//----------------------------------------------------------------------------

TEST(ConfigParser, GetAlLSections)
{
  string s = R"(
      [s1]
      k1 = ssldfjsljf
      k2 = 43234
      [s2]
      k3 = -3.14
      k4 = off
      [s3]
      k5 = /usr/bin
      k6 = 2018-07-15
             )";

  istringstream is{s};
  Parser cp(is);

  vector<string> as = cp.allSections();
  ASSERT_EQ(4, as.size());
  for (const string& s : {"s1", "s2", "s3", "__DEFAULT__"})
  {
    auto it = std::find(as.cbegin(), as.cend(), s);
    ASSERT_TRUE(it != as.cend());
  }
}
