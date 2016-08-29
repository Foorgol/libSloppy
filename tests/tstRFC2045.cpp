#include <iostream>

#include <gtest/gtest.h>

#include "../Sloppy/MailAndMIME/MIME_Message.h"

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

