#include <string>

#include <gtest/gtest.h>

#include "../Sloppy/libSloppy.h"

using namespace std;

TEST(EmailPattern, PatternCheck)
{
  ASSERT_FALSE(Sloppy::isValidEmailAddress(""));
  ASSERT_FALSE(Sloppy::isValidEmailAddress(" "));
  ASSERT_FALSE(Sloppy::isValidEmailAddress(" abc@123.org "));
  ASSERT_FALSE(Sloppy::isValidEmailAddress("abc"));
  ASSERT_FALSE(Sloppy::isValidEmailAddress("abc@"));
  ASSERT_FALSE(Sloppy::isValidEmailAddress("abc @"));
  ASSERT_FALSE(Sloppy::isValidEmailAddress("abc@123"));
  ASSERT_FALSE(Sloppy::isValidEmailAddress("ab cd@123.org"));
  ASSERT_FALSE(Sloppy::isValidEmailAddress("@123.org"));
  ASSERT_FALSE(Sloppy::isValidEmailAddress("abc@x.y"));
  ASSERT_TRUE(Sloppy::isValidEmailAddress("abc@xx.yz"));
  ASSERT_TRUE(Sloppy::isValidEmailAddress("abc@123.org"));
  ASSERT_TRUE(Sloppy::isValidEmailAddress("abc_de.fgh@123.456.org"));
}
