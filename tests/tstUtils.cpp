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

#include <string>

#include <gtest/gtest.h>

#include "../Sloppy/Utils.h"
#include "../Sloppy/json.hpp"
#include "../Sloppy/Memory.h"

using namespace std;

TEST(Utils, EmailPatternCheck)
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

//----------------------------------------------------------------------------

TEST(Utils, AssignIfNotNull)
{
  int x = 42;
  int* px = &x;

  ASSERT_EQ(42, *px);
  ASSERT_EQ(42, x);
  ASSERT_NO_THROW(Sloppy::assignIfNotNull<int>(px, 666));
  ASSERT_EQ(666, *px);
  ASSERT_EQ(666, x);

  px = nullptr;
  ASSERT_NO_THROW(Sloppy::assignIfNotNull<int>(px, 666));
  ASSERT_EQ(666, x);
}

//----------------------------------------------------------------------------

TEST(Utils, IsInVector)
{
  vector<int> v{1,2,3,4,5};
  ASSERT_TRUE(Sloppy::isInVector<int>(v, 3));
  ASSERT_FALSE(Sloppy::isInVector<int>(v, 6));

  v = vector<int>{};
  ASSERT_FALSE(Sloppy::isInVector<int>(v, 3));
}

//----------------------------------------------------------------------------

TEST(Utils, EraseFromVector)
{
  vector<int> v{1,2,5,4,5};
  ASSERT_EQ(2, Sloppy::eraseAllOccurencesFromVector<int>(v, 5));
  vector<int> vRef{1,2,4};
  ASSERT_EQ(vRef, v);

  ASSERT_EQ(0, Sloppy::eraseAllOccurencesFromVector<int>(v, 5));
  ASSERT_EQ(vRef, v);

  v = vector<int>{};
  vRef = vector<int>{};
  ASSERT_EQ(0, Sloppy::eraseAllOccurencesFromVector<int>(v, 5));
  ASSERT_EQ(vRef, v);

  v = vector<int>{5};
  ASSERT_EQ(1, Sloppy::eraseAllOccurencesFromVector<int>(v, 5));
  ASSERT_EQ(vRef, v);

  v = vector<int>{5,5,5};
  ASSERT_EQ(3, Sloppy::eraseAllOccurencesFromVector<int>(v, 5));
  ASSERT_EQ(vRef, v);
}

//----------------------------------------------------------------------------

TEST(Utils, TrimString) {
  const std::vector<std::vector<std::string>> tstData = {
    {"abc", "abc", "abc", "abc"},
    {"ab c", "ab c", "ab c", "ab c"},
    {" abc", "abc", " abc", "abc"},
    {"  abc", "abc", "  abc", "abc"},
    {"abc ", "abc ", "abc", "abc"},
    {"abc  ", "abc  ", "abc", "abc"},
    {"\tabc ", "abc ", "\tabc", "abc"},
    {"\tabc\r", "abc\r", "\tabc", "abc"},
    {"", "", "", ""},
    {" ", "", "", ""},
    {"\t\r\n", "", "", ""},
  };

  for (const auto& sArray : tstData) {
    std::string s = sArray[0];
    Sloppy::trimLeft(s);
    ASSERT_EQ(sArray[1], s);

    s = sArray[0];
    Sloppy::trimRight(s);
    ASSERT_EQ(sArray[2], s);

    s = sArray[0];
    Sloppy::trim(s);
    ASSERT_EQ(sArray[3], s);
  }
}

//----------------------------------------------------------------------------

TEST(Utils, TrimAndCheckString)
{
  string s = " 123 ";
  ASSERT_TRUE(Sloppy::trimAndCheckString(s, 10));
  ASSERT_EQ("123", s);

  s = " 123 ";
  ASSERT_FALSE(Sloppy::trimAndCheckString(s, 1));
  ASSERT_EQ("123", s);

  s = "123";
  ASSERT_TRUE(Sloppy::trimAndCheckString(s));
  ASSERT_EQ("123", s);

  // strings shall be non-empty
  s = "";
  ASSERT_FALSE(Sloppy::trimAndCheckString(s));
  ASSERT_EQ("", s);
  ASSERT_FALSE(Sloppy::trimAndCheckString(s, 20));
  ASSERT_EQ("", s);
}

//----------------------------------------------------------------------------

TEST(Utils, Json2String)
{
  using json = nlohmann::json;

  json j;
  ASSERT_EQ(json::value_t::null, j.type());
  ASSERT_EQ("", Sloppy::json2String(j));

  j = 42u;
  ASSERT_EQ(json::value_t::number_unsigned, j.type());
  ASSERT_EQ("42", Sloppy::json2String(j));

  j = 42;
  ASSERT_EQ(json::value_t::number_integer, j.type());
  ASSERT_EQ("42", Sloppy::json2String(j));

  j = -42;
  ASSERT_EQ(json::value_t::number_integer, j.type());
  ASSERT_EQ("-42", Sloppy::json2String(j));

  j = -42.666;
  ASSERT_EQ(json::value_t::number_float, j.type());
  ASSERT_EQ("-42.666", Sloppy::json2String(j, 3));
  ASSERT_EQ("-42.66600", Sloppy::json2String(j, 5));
  ASSERT_EQ("-42.7", Sloppy::json2String(j, 1));

  j = true;
  ASSERT_EQ(json::value_t::boolean, j.type());
  ASSERT_EQ("1", Sloppy::json2String(j));

  j = false;
  ASSERT_EQ(json::value_t::boolean, j.type());
  ASSERT_EQ("0", Sloppy::json2String(j));

  j = "I am a string";
  ASSERT_EQ(json::value_t::string, j.type());
  ASSERT_EQ("I am a string", Sloppy::json2String(j));

  j = {1,2,3};
  ASSERT_EQ(json::value_t::array, j.type());
  ASSERT_THROW(Sloppy::json2String(j), std::invalid_argument);

  j = json::object();
  j["dyf"] = "dkf";
  j["mb"] = 76575;
  ASSERT_EQ(json::value_t::object, j.type());
  ASSERT_THROW(Sloppy::json2String(j), std::invalid_argument);

}

//----------------------------------------------------------------------------

TEST(Utils, JsonObjectHasKey)
{
  using json = nlohmann::json;

  json j = json::object();
  ASSERT_FALSE(Sloppy::jsonObjectHasKey(j, "key", json::value_t::number_integer));

  j["sdfsdf"] = "fsfdlks";
  ASSERT_FALSE(Sloppy::jsonObjectHasKey(j, "key", json::value_t::number_integer));

  j["key"] = "abc";
  ASSERT_FALSE(Sloppy::jsonObjectHasKey(j, "key", json::value_t::number_integer));

  j["key"] = 1.111;
  ASSERT_FALSE(Sloppy::jsonObjectHasKey(j, "key", json::value_t::number_integer));

  j["key"] = 42;
  ASSERT_TRUE(Sloppy::jsonObjectHasKey(j, "key", json::value_t::number_integer));

  j = 42;
  ASSERT_FALSE(Sloppy::jsonObjectHasKey(j, "key", json::value_t::number_integer));

  j = "abc";
  ASSERT_FALSE(Sloppy::jsonObjectHasKey(j, "key", json::value_t::number_integer));

  json j_null;
  ASSERT_FALSE(Sloppy::jsonObjectHasKey(j_null, "key", json::value_t::number_integer));
}
//----------------------------------------------------------------------------

TEST(Utils, BiDirPipe)
{
  auto pipePair= Sloppy::createBirectionalPipe();
  Sloppy::BiDirPipeEnd e1{std::move(pipePair.first)};
  Sloppy::BiDirPipeEnd e2{std::move(pipePair.second)};

  // test direction one
  ASSERT_TRUE(e1.blockingWrite("FirstDirection"));
  Sloppy::MemArray data = e2.blockingRead_FixedSize(14, 10);
  string sDat{data.to_charPtr(), data.size()};
  ASSERT_EQ("FirstDirection", sDat);

  // test direction two
  ASSERT_TRUE(e2.blockingWrite("OtherDirection"));
  data = e1.blockingRead_FixedSize(14, 10);
  sDat = string{data.to_charPtr(), data.size()};
  ASSERT_EQ("OtherDirection", sDat);
}

//----------------------------------------------------------------------------

TEST(Utils, SimplePipe)
{
  Sloppy::ManagedFileDescriptor r;
  Sloppy::ManagedFileDescriptor w;
  tie(r, w) = Sloppy::createSimplePipe();

  // test data exchance
  ASSERT_TRUE(w.blockingWrite("abcd"));
  Sloppy::MemArray data = r.blockingRead_FixedSize(4, 10);
  string sDat{data.to_charPtr(), data.size()};
  ASSERT_EQ("abcd", sDat);
}

//----------------------------------------------------------------------------

TEST(Utils, CommaSepListFromVals)
{
  ASSERT_EQ("1,2,3", Sloppy::commaSepStringFromValues({1,2,3}));
  ASSERT_EQ("42", Sloppy::commaSepStringFromValues({42}));
  ASSERT_EQ("", Sloppy::commaSepStringFromValues<int>({}));
  ASSERT_EQ("123", Sloppy::commaSepStringFromValues({1,2,3}, ""));
  ASSERT_EQ("1x2x3", Sloppy::commaSepStringFromValues({1,2,3}, "x"));

  ASSERT_EQ("a,b,c", Sloppy::commaSepStringFromValues({"a", "b", "c"}));

  vector<int> v{1,2,3};
  ASSERT_EQ("1,2,3", Sloppy::commaSepStringFromValues(v));

  vector<string> vs{"a", "", "b", "c"};
  ASSERT_EQ("axxbxc", Sloppy::commaSepStringFromValues(vs, "x"));

}


//----------------------------------------------------------------------------

TEST(Utils, ZeroPadding)
{
  ASSERT_EQ("001", Sloppy::zeroPaddedNumber(1, 3));
  ASSERT_EQ("-0042", Sloppy::zeroPaddedNumber(-42, 4));
  ASSERT_EQ("001", Sloppy::zeroPaddedNumber(1l, 3));
  ASSERT_EQ("-0042", Sloppy::zeroPaddedNumber(-42l, 4));
  ASSERT_EQ("-42", Sloppy::zeroPaddedNumber(-42, 0));
  ASSERT_EQ("-42", Sloppy::zeroPaddedNumber(-42, -1));
  ASSERT_EQ("0001234", Sloppy::zeroPaddedNumber(1234u, 7));
}
