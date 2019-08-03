/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2019  Volker Knollmann
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
#include <variant>

#include <gtest/gtest.h>

#include "../Sloppy/CSV.h"

using namespace std;

TEST(Utils, CSV_Val_Ctor)
{
  Sloppy::CSV_Value v0;
  ASSERT_EQ(Sloppy::CSV_Value::Type::Null, v0.valueType());
  ASSERT_FALSE(v0.has_value());

  Sloppy::CSV_Value v1{42};
  ASSERT_EQ(Sloppy::CSV_Value::Type::Long, v1.valueType());
  ASSERT_TRUE(v1.has_value());
  ASSERT_EQ(42, v1.get<long>());

  Sloppy::CSV_Value v2{42.4242};
  ASSERT_EQ(Sloppy::CSV_Value::Type::Double, v2.valueType());
  ASSERT_TRUE(v2.has_value());
  ASSERT_EQ(42.4242, v2.get<double>());

  Sloppy::CSV_Value v3{"CSV!"};
  ASSERT_EQ(Sloppy::CSV_Value::Type::String, v3.valueType());
  ASSERT_TRUE(v3.has_value());
  ASSERT_EQ("CSV!", v3.get<std::string>());

  std::string moveTest{"move it!"};
  ASSERT_FALSE(moveTest.empty());
  Sloppy::CSV_Value v4{std::move(moveTest)};
  ASSERT_TRUE(moveTest.empty());
  ASSERT_EQ(Sloppy::CSV_Value::Type::String, v4.valueType());
  ASSERT_TRUE(v4.has_value());
  ASSERT_EQ("move it!", v4.get<std::string>());
}

//----------------------------------------------------------------------------

TEST(Utils, CSV_Val_Set)
{
  Sloppy::CSV_Value v;

  v.set(42);
  ASSERT_EQ(Sloppy::CSV_Value::Type::Long, v.valueType());
  ASSERT_TRUE(v.has_value());
  ASSERT_EQ(42, v.get<long>());

  v.set();
  ASSERT_EQ(Sloppy::CSV_Value::Type::Null, v.valueType());
  ASSERT_FALSE(v.has_value());

  v.set(666.666);
  ASSERT_EQ(Sloppy::CSV_Value::Type::Double, v.valueType());
  ASSERT_TRUE(v.has_value());
  ASSERT_EQ(666.666, v.get<double>());

  v.set("abc123");
  ASSERT_EQ(Sloppy::CSV_Value::Type::String, v.valueType());
  ASSERT_TRUE(v.has_value());
  ASSERT_EQ("abc123", v.get<std::string>());

  std::string moveTest{"move it!"};
  ASSERT_FALSE(moveTest.empty());
  v.set(std::move(moveTest));
  ASSERT_TRUE(moveTest.empty());
  ASSERT_EQ(Sloppy::CSV_Value::Type::String, v.valueType());
  ASSERT_TRUE(v.has_value());
  ASSERT_EQ("move it!", v.get<std::string>());
}

//----------------------------------------------------------------------------

TEST(Utils, CSV_Val_StringConversion)
{
  using Rep = Sloppy::CSV_StringRepresentation;

  Sloppy::CSV_Value v;

  // empty value
  std::string s = v.asString(Rep::Quoted);
  ASSERT_TRUE(s.empty());
  s = v.asString(Rep::QuotedAndEscaped);
  ASSERT_TRUE(s.empty());
  ASSERT_THROW(v.asString(Rep::Plain), std::bad_optional_access);
  ASSERT_THROW(v.asString(Rep::Escaped), std::bad_optional_access);

  v.set(42);
  ASSERT_EQ("42", v.asString(Rep::Plain));
  ASSERT_EQ("42", v.asString(Rep::Quoted));
  ASSERT_EQ("42", v.asString(Rep::Escaped));
  ASSERT_EQ("42", v.asString(Rep::QuotedAndEscaped));

  v.set(23.23);
  ASSERT_EQ("23.230000", v.asString(Rep::Plain));
  ASSERT_EQ("23.230000", v.asString(Rep::Quoted));
  ASSERT_EQ("23.230000", v.asString(Rep::Escaped));
  ASSERT_EQ("23.230000", v.asString(Rep::QuotedAndEscaped));

  v.set("abc");
  ASSERT_EQ("abc", v.asString(Rep::Plain));
  ASSERT_EQ("\"abc\"", v.asString(Rep::Quoted));
  ASSERT_EQ("abc", v.asString(Rep::Escaped));
  ASSERT_EQ("\"abc\"", v.asString(Rep::QuotedAndEscaped));

  v.set("x,y");
  ASSERT_EQ("x,y", v.asString(Rep::Plain));
  ASSERT_EQ("\"x,y\"", v.asString(Rep::Quoted));
  ASSERT_EQ("x\\,y", v.asString(Rep::Escaped));
  ASSERT_EQ("\"x\\,y\"", v.asString(Rep::QuotedAndEscaped));
}

//----------------------------------------------------------------------------

TEST(Utils, CSV_Row_Ctor)
{
  using Rep = Sloppy::CSV_StringRepresentation;

  Sloppy::CSV_Row r0;
  ASSERT_EQ(0, r0.size());

  auto tstRow = [](const std::vector<std::pair<char, std::string>>& tstData, const Sloppy::CSV_Row& r)
  {
    ASSERT_EQ(tstData.size(), r.size());

    for (size_t idx=0; idx < r.size(); ++idx)
    {
      const auto& val = r.get(idx);

      auto [dataType, data] = tstData[idx];

      long l;
      double d;
      switch (dataType)
      {
        case 'n':
          ASSERT_EQ(Sloppy::CSV_Value::Type::Null, val.valueType());
          continue;

        case 'l':
          ASSERT_EQ(Sloppy::CSV_Value::Type::Long, val.valueType());
          l = stol(data);
          ASSERT_EQ(l, val.get<long>());
          continue;

        case 'd':
          ASSERT_EQ(Sloppy::CSV_Value::Type::Double, val.valueType());
          d = stod(data);
          ASSERT_EQ(d, val.get<double>());
          continue;

        case 's':
          ASSERT_EQ(Sloppy::CSV_Value::Type::String, val.valueType());
          ASSERT_EQ(data, val.get<string>());
          continue;

        default:
          ASSERT_TRUE(false);
      }
    }
  };

  std::string tstString{"1,2,,  ,\"5\", \\,,\"12\\\"34\""};

  Sloppy::CSV_Row r1{tstString, Rep::Plain};
  std::vector<std::pair<char, std::string>> tstData{
    {'l', "1"},
    {'l', "2"},
    {'n', ""},
    {'s', "  "},
    {'s', "\"5\""},
    {'s', " \\"},
    {'n', ""},
    {'s', "\"12\\\"34\""},
  };
  tstRow(tstData, r1);

  Sloppy::CSV_Row r2{tstString, Rep::Escaped};
  tstData = std::vector<std::pair<char, std::string>> {
    {'l', "1"},
    {'l', "2"},
    {'n', ""},
    {'s', "  "},
    {'s', "\"5\""},
    {'s', " ,"},
    {'s', "\"12\"34\""},
  };
  tstRow(tstData, r2);

  // the un-quoted backslash in the input string
  // prevents parsing this string als "Quoted" or "QuotedAndEscaped"
  ASSERT_THROW(Sloppy::CSV_Row(tstString, Rep::Quoted), std::invalid_argument);
  ASSERT_THROW(Sloppy::CSV_Row(tstString, Rep::QuotedAndEscaped), std::invalid_argument);

  // try some other strings for better test
  // coverage of quoted / escaped strings
  tstString = R"(,",",)";
  Sloppy::CSV_Row r4{tstString, Rep::Quoted};
  tstData = std::vector<std::pair<char, std::string>> {
    {'n', ""},
    {'s', ","},
    {'n', ""},
  };
  tstRow(tstData, r4);

  ASSERT_THROW(Sloppy::CSV_Row(R"(,","",)", Rep::Quoted), std::invalid_argument);
  ASSERT_THROW(Sloppy::CSV_Row(R"(,","xyz,)", Rep::Quoted), std::invalid_argument);

  r4 = Sloppy::CSV_Row {R"(,"","")", Rep::Quoted};
  tstData = std::vector<std::pair<char, std::string>> {
    {'n', ""},
    {'s', ""},
    {'s', ""},
  };
  tstRow(tstData, r4);

  r4 = Sloppy::CSV_Row {R"(,"","")", Rep::Quoted};
  tstData = std::vector<std::pair<char, std::string>> {
    {'n', ""},
    {'s', ""},
    {'s', ""},
  };
  tstRow(tstData, r4);

  r4 = Sloppy::CSV_Row {R"(,"\"","")", Rep::QuotedAndEscaped};
  tstData = std::vector<std::pair<char, std::string>> {
    {'n', ""},
    {'s', "\""},
    {'s', ""},
  };
  tstRow(tstData, r4);

  ASSERT_THROW(Sloppy::CSV_Row(R"(,"\""\","")", Rep::QuotedAndEscaped), std::invalid_argument);
  ASSERT_THROW(Sloppy::CSV_Row(R"(x)", Rep::QuotedAndEscaped), std::invalid_argument);

  r4 = Sloppy::CSV_Row {"", Rep::QuotedAndEscaped};
  tstData = std::vector<std::pair<char, std::string>> {
  };
  tstRow(tstData, r4);

  r4 = Sloppy::CSV_Row {",", Rep::QuotedAndEscaped};
  tstData = std::vector<std::pair<char, std::string>> {
    {'n', ""},
    {'n', ""},
  };
  tstRow(tstData, r4);

  r4 = Sloppy::CSV_Row {R"(  1 ,        "ab"     ,   2.2)", Rep::QuotedAndEscaped};
  tstData = std::vector<std::pair<char, std::string>> {
    {'l', "1"},
    {'s', "ab"},
    {'d', "2.2"},
  };
  tstRow(tstData, r4);

  r4 = Sloppy::CSV_Row {R"(  -1 ,        ab     ,   2.2)", Rep::Plain};
  tstData = std::vector<std::pair<char, std::string>> {
    {'l', "-1"},
    {'s', "        ab     "},
    {'d', "2.2"},
  };
  tstRow(tstData, r4);

}

//----------------------------------------------------------------------------

TEST(Utils, CSV_Row_Append)
{
  using Rep = Sloppy::CSV_StringRepresentation;

  Sloppy::CSV_Row r;
  ASSERT_TRUE(r.empty());

  r.append(42.42);
  ASSERT_EQ(1, r.size());
  ASSERT_EQ(Sloppy::CSV_Value::Type::Double, r[0].valueType());
  ASSERT_EQ(42.42, r[0].get<double>());

  r.append("abc,");
  ASSERT_EQ(2, r.size());
  ASSERT_EQ(Sloppy::CSV_Value::Type::String, r[1].valueType());
  ASSERT_EQ("abc,", r[1].get<string>());

  r.append("");
  ASSERT_EQ(3, r.size());
  ASSERT_EQ(Sloppy::CSV_Value::Type::String, r[2].valueType());
  ASSERT_EQ("", r[2].get<string>());

  r.append();
  ASSERT_EQ(4, r.size());
  ASSERT_EQ(Sloppy::CSV_Value::Type::Null, r[3].valueType());

  r.append(-9);
  ASSERT_EQ(5, r.size());
  ASSERT_EQ(Sloppy::CSV_Value::Type::Long, r[4].valueType());
  ASSERT_EQ(-9, r[4].get<long>());

  ASSERT_EQ(R"(42.420000,"abc,","",,-9)", r.asString(Rep::Quoted));
  ASSERT_EQ(R"(42.420000,"abc\,","",,-9)", r.asString(Rep::QuotedAndEscaped));
}

//----------------------------------------------------------------------------

TEST(Utils, CSV_Row_AsString)
{
  using Rep = Sloppy::CSV_StringRepresentation;

  Sloppy::CSV_Row r;
  ASSERT_EQ("", r.asString(Rep::Plain));
  ASSERT_EQ("", r.asString(Rep::Escaped));
  ASSERT_EQ("", r.asString(Rep::Quoted));
  ASSERT_EQ("", r.asString(Rep::QuotedAndEscaped));

  r.append();
  ASSERT_THROW(r.asString(Rep::Plain), std::bad_optional_access);
  ASSERT_THROW(r.asString(Rep::Escaped), bad_optional_access);
  ASSERT_EQ("", r.asString(Rep::Quoted));
  ASSERT_EQ("", r.asString(Rep::QuotedAndEscaped));

  r.append();
  ASSERT_THROW(r.asString(Rep::Plain), std::bad_optional_access);
  ASSERT_THROW(r.asString(Rep::Escaped), bad_optional_access);
  ASSERT_EQ(",", r.asString(Rep::Quoted));
  ASSERT_EQ(",", r.asString(Rep::QuotedAndEscaped));

  r.append(123);
  ASSERT_THROW(r.asString(Rep::Plain), std::bad_optional_access);
  ASSERT_THROW(r.asString(Rep::Escaped), bad_optional_access);
  ASSERT_EQ(",,123", r.asString(Rep::Quoted));
  ASSERT_EQ(",,123", r.asString(Rep::QuotedAndEscaped));

  r.append("ab,c");
  ASSERT_THROW(r.asString(Rep::Plain), std::bad_optional_access);
  ASSERT_THROW(r.asString(Rep::Escaped), bad_optional_access);
  ASSERT_EQ(",,123,\"ab,c\"", r.asString(Rep::Quoted));
  ASSERT_EQ(",,123,\"ab\\,c\"", r.asString(Rep::QuotedAndEscaped));
}

//----------------------------------------------------------------------------

TEST(Utils, CSV_Tab_Ctor)
{
  using Rep = Sloppy::CSV_StringRepresentation;

  Sloppy::CSV_Table t;
  ASSERT_TRUE(t.empty());
  ASSERT_FALSE(t.hasHeaders());
  ASSERT_EQ(0, t.nCols());

  t = Sloppy::CSV_Table("a,b,c\n1,2.2,x", true, Rep::Plain);
  ASSERT_FALSE(t.empty());
  ASSERT_TRUE(t.hasHeaders());
  ASSERT_EQ(3, t.nCols());
  ASSERT_EQ(1, t.size());
  ASSERT_EQ("a", t.getHeader(0));
  ASSERT_EQ("b", t.getHeader(1));
  ASSERT_EQ("c", t.getHeader(2));
  ASSERT_EQ(1, t.get(0,0).get<long>());
  ASSERT_EQ(2.2, t.get(0,"b").get<double>());
  ASSERT_EQ("x", t.get(0,2).get<string>());

  ASSERT_THROW(Sloppy::CSV_Table("a,b,c\n1,2.2,x,y", true, Rep::Plain), std::invalid_argument);
  ASSERT_THROW(Sloppy::CSV_Table("a,b,c\n1,2.2", true, Rep::Plain), std::invalid_argument);

  t = Sloppy::CSV_Table("a", true, Rep::Plain);
  ASSERT_TRUE(t.empty());
  ASSERT_TRUE(t.hasHeaders());
  ASSERT_EQ(1, t.nCols());
  ASSERT_EQ(0, t.size());
  ASSERT_EQ("a", t.getHeader(0));
  ASSERT_THROW(t.getHeader(1), std::out_of_range);
}

//----------------------------------------------------------------------------

TEST(Utils, CSV_Tab_Append)
{
  using Rep = Sloppy::CSV_StringRepresentation;

  Sloppy::CSV_Table t;
  Sloppy::CSV_Row r{"1, 2, 3.14,   4,x", Rep::Plain};
  ASSERT_TRUE(t.append(r));
  ASSERT_FALSE(t.empty());
  ASSERT_FALSE(t.hasHeaders());
  ASSERT_EQ(5, t.nCols());
  ASSERT_EQ(1, t.size());

  r = Sloppy::CSV_Row{"42", Rep::Plain};
  ASSERT_FALSE(t.append(r));
  r.append(1);
  ASSERT_FALSE(t.append(r));
  r.append(2);
  ASSERT_FALSE(t.append(r));
  r.append(3);
  ASSERT_FALSE(t.append(r));
  r.append(4);
  ASSERT_TRUE(t.append(r));
  ASSERT_EQ("x", t.get(0,4).get<string>());
  ASSERT_EQ(2, t.get(1,2).get<long>());
  ASSERT_EQ(2, t.size());
}

//----------------------------------------------------------------------------

TEST(Utils, CSV_Tab_Headers)
{
  using Rep = Sloppy::CSV_StringRepresentation;

  Sloppy::CSV_Table t;
  Sloppy::CSV_Row r{"1, 2, 3.14,   4,x", Rep::Plain};
  ASSERT_TRUE(t.append(r));

  ASSERT_FALSE(t.setHeader(2, "xy"));
  ASSERT_FALSE(t.hasHeaders());

  ASSERT_TRUE(t.setHeader(std::vector<string>{"a", "b", "c", "d", "e"}));
  ASSERT_TRUE(t.hasHeaders());
  ASSERT_EQ("a", t.getHeader(0));
  ASSERT_EQ("b", t.getHeader(1));
  ASSERT_EQ("c", t.getHeader(2));
  ASSERT_EQ("d", t.getHeader(3));
  ASSERT_EQ("e", t.getHeader(4));

  r = Sloppy::CSV_Row{"100,200,300,\"hhh\",400", Rep::QuotedAndEscaped};
  ASSERT_TRUE(t.setHeader(r));
  ASSERT_EQ("100", t.getHeader(0));
  ASSERT_EQ("200", t.getHeader(1));
  ASSERT_EQ("300", t.getHeader(2));
  ASSERT_EQ("hhh", t.getHeader(3));
  ASSERT_EQ("400", t.getHeader(4));

  for (const string& s : {"100", "", ",", "ab\"xy", "\""})
  {
    ASSERT_FALSE(t.setHeader(2, s));
    ASSERT_EQ("300", t.getHeader(2));
  }
}

//----------------------------------------------------------------------------

TEST(Utils, CSV_Tab_StringExport)
{
  using Rep = Sloppy::CSV_StringRepresentation;

  Sloppy::CSV_Table t;
  ASSERT_EQ("", t.asString(true, Rep::QuotedAndEscaped));

  Sloppy::CSV_Row r{"1, 2, 3.14,   4, x", Rep::Plain};
  ASSERT_TRUE(t.append(r));
  ASSERT_EQ("1,2,3.140000,4,\" x\"\n", t.asString(true, Rep::QuotedAndEscaped));

  r = Sloppy::CSV_Row{",,abc,,", Rep::Plain};
  ASSERT_TRUE(t.append(r));
  ASSERT_TRUE(t.setHeader(std::vector<string>{"a", "b", "c", "d", "e"}));
  string expected{R"(1,2,3.140000,4," x")"};
  expected += "\n";
  expected += ",,\"abc\",,";
  expected += "\n";
  ASSERT_EQ(expected, t.asString(false, Rep::QuotedAndEscaped));

  expected = R"("a","b","c","d","e")" + ("\n" + expected);
  ASSERT_EQ(expected, t.asString(true, Rep::QuotedAndEscaped));
}

//----------------------------------------------------------------------------

TEST(Utils, CSV_Tab_EraseColumn)
{
  using Rep = Sloppy::CSV_StringRepresentation;

  Sloppy::CSV_Table t;
  ASSERT_FALSE(t.eraseColumn(0));

  ASSERT_TRUE(t.setHeader(std::vector<string>{"a", "b", "c", "d", "e"}));
  Sloppy::CSV_Row r{"1, 2, 3.14,   4, x", Rep::Plain};
  ASSERT_TRUE(t.append(r));
  r = Sloppy::CSV_Row{"99,98,97,96,100", Rep::Plain};
  ASSERT_TRUE(t.append(r));

  ASSERT_EQ(5, t.nCols());
  ASSERT_TRUE(t.eraseColumn(3));
  ASSERT_EQ(4, t.nCols());
  ASSERT_EQ(3.14, t.get(0,2).get<double>());
  ASSERT_EQ(97, t.get(1,2).get<long>());
  ASSERT_EQ(" x", t.get(0,3).get<string>());
  ASSERT_EQ(100, t.get(1,3).get<long>());
  ASSERT_EQ("c", t.getHeader(2));
  ASSERT_EQ("e", t.getHeader(3));

  // boundary case: last column
  ASSERT_TRUE(t.eraseColumn("e"));
  ASSERT_EQ(3, t.nCols());
  ASSERT_EQ("c", t.getHeader(2));
  ASSERT_EQ(3.14, t.get(0,2).get<double>());
  ASSERT_EQ(97, t.get(1,2).get<long>());

  // boundary case: first column
  ASSERT_TRUE(t.eraseColumn(0));
  ASSERT_EQ(2, t.nCols());
  ASSERT_EQ("b", t.getHeader(0));
  ASSERT_EQ(2, t.get(0,0).get<long>());
  ASSERT_EQ(98, t.get(1,0).get<long>());

  // invalid column
  ASSERT_FALSE(t.eraseColumn(99));
  ASSERT_FALSE(t.eraseColumn("xyz"));
  ASSERT_EQ(2, t.nCols());
}

//----------------------------------------------------------------------------

TEST(Utils, CSV_Erase)
{
  using Rep = Sloppy::CSV_StringRepresentation;

  Sloppy::CSV_Row r;
  r.append(42);
  r.append(23);
  r.append(666);
  auto rowIt = r.begin();
  ++rowIt;
  r.erase(rowIt);
  ASSERT_EQ(2, r.size());
  ASSERT_EQ(666, r[1].get<long>());

  Sloppy::CSV_Table t;
  t.append(r);
  t.append(r);
  t.append(r);
  auto tabIt = t.cbegin();
  ++tabIt;
  t.erase(tabIt);
  ASSERT_EQ(2, t.size());
}
