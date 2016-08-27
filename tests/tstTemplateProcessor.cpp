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
  t.getSubstitutedData(d, out, "##", "#");
  ASSERT_EQ("abc 1 def 2", out);

  // check keys that are part of each other
  string s = "123 a_bc_de 123 bc 123";
  t = Template{s};
  d = SubstDic{{"bc", "x"}, {"a_bc_de", "y"}};
  t.getSubstitutedData(d, out);
  ASSERT_EQ("123 y 123 x 123", out);
  d = SubstDic{{"a_bc_de", "y"}, {"bc", "x"}};  // test internal sorting
  t.getSubstitutedData(d, out);
  ASSERT_EQ("123 y 123 x 123", out);
}

//----------------------------------------------------------------------------

TEST(TemplateProcessor, TemplateCollectionFunc)
{
  // create a dummy template collection
  TemplateCollection tc;
  ASSERT_TRUE(tc.addTemplate("T1: abc var1 def", "t1"));
  ASSERT_TRUE(tc.addTemplate("T2: abc var1 def", "t2"));
  ASSERT_FALSE(tc.addTemplate("lalala", "t2"));

  // test substitutions
  SubstDic d{{"var1", "1"}, {"var2", "2"}};
  string out;
  ASSERT_TRUE(tc.getSubstitutedData("t1", d, out));
  ASSERT_EQ("T1: abc 1 def", out);
  ASSERT_TRUE(tc.getSubstitutedData("t2", d, out));
  ASSERT_EQ("T2: abc 1 def", out);
  ASSERT_FALSE(tc.getSubstitutedData("sfdkjsdkf", d, out));
}
