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

#include <iostream>

#include <gtest/gtest.h>

#include "../Sloppy/MailAndMIME/MIME_Message.h"

using namespace std;

TEST(MIME_Message, StructuredHeaderBody)
{
  string h = "some value; name1=a;name2=q87645---";
  Sloppy::RFC2045::StructuredHeaderBody b{h};
  ASSERT_EQ("some value", b.getValue());
  ASSERT_TRUE(b.hasParameter("Name1"));
  ASSERT_TRUE(b.hasParameter("name1"));
  ASSERT_TRUE(b.hasParameter("NAME2"));
  ASSERT_EQ("a", b.getParameter("name1"));
  ASSERT_EQ("q87645---", b.getParameter("name2"));

  h = "some value; name1=\"quoted String\"";
  b = Sloppy::RFC2045::StructuredHeaderBody{h};
  ASSERT_TRUE(b.hasParameter("Name1"));
  ASSERT_EQ("quoted String", b.getParameter("name1"));  // the quotes are NOT part of the value!

  h = "some value; name1=\"quoted String with ; and more\"";
  b = Sloppy::RFC2045::StructuredHeaderBody{h};
  ASSERT_TRUE(b.hasParameter("Name1"));
  ASSERT_EQ("quoted String with ; and more", b.getParameter("name1"));  // the quotes are NOT part of the value!
}

//----------------------------------------------------------------------------

