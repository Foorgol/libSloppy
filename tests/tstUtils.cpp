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

#include <string>

#include <gtest/gtest.h>

#include "../Sloppy/Utils.h"

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
