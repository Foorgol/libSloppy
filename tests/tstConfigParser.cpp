#include <iostream>
#include <sstream>

#include <gtest/gtest.h>

#include "../Sloppy/ConfigFileParser/ConfigFileParser.h"
#include "BasicTestClass.h"

using namespace Sloppy::ConfigFileParser;

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
  s << "var3 = x";  // no endl here to test that the last line is parsed as well!

  // parse it
  auto cp = Parser::read(s.str());

  // check it
  ASSERT_TRUE(cp != nullptr);
  ASSERT_TRUE(cp->hasKey("var1"));
  ASSERT_EQ("42", cp->getValue("var1"));  // "1" should have been overwritten!
  ASSERT_EQ("x", cp->getValue("var3"));
  ASSERT_EQ("", cp->getValue("Var3"));
  ASSERT_EQ("ab cd/ef\\g", cp->getValue("var 2"));
  ASSERT_EQ("=4=5", cp->getValue("3"));

  // test conversion to int
  bool ok = false;
  ASSERT_EQ(42, cp->getValueAsInt("var1", -1, &ok));
  ASSERT_TRUE(ok);
  ASSERT_EQ(-1, cp->getValueAsInt("var3", -1, &ok));  // conversion of a 'x'
  ASSERT_FALSE(ok);
  ok = true;
  ASSERT_EQ(-1, cp->getValueAsInt("Var3", -1, &ok));  // conversion of an empty string
  ASSERT_FALSE(ok);
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
  auto cp = Parser::read(s.str());

  // check it
  ASSERT_TRUE(cp != nullptr);
  ASSERT_TRUE(cp->hasKey("var1"));
  ASSERT_EQ("0", cp->getValue("var1"));
  ASSERT_EQ("1", cp->getValue("Sec1", "var1"));
  ASSERT_EQ("88", cp->getValue("Sec1", "var2"));
  ASSERT_EQ("2", cp->getValue("Sec2", "var1"));
  ASSERT_TRUE(cp->hasSection("Sec3"));
  ASSERT_TRUE(cp->hasSection("Sec with spaces"));
  ASSERT_FALSE(cp->hasSection("Garb"));
}
