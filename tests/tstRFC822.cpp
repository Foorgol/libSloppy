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
#include <fstream>

#include <gtest/gtest.h>

#include "../Sloppy/MailAndMIME/Message.h"
#include "../Sloppy/MailAndMIME/Header.h"
#include "BasicTestClass.h"

using namespace Sloppy::RFC822;

class EmailFixture : public BasicTestFixture
{
public:
  string getMailData() const
  {
    return rawMsg;
  }

protected:
  virtual void SetUp() override
  {
    ifstream f{"../tests/rawEmail.txt", ifstream::binary};
    rawMsg = string{(istreambuf_iterator<char>(f)), istreambuf_iterator<char>()};
  }

private:
  string rawMsg;
};

//----------------------------------------------------------------------------

TEST_F(EmailFixture, MessageCtor)
{
  Message msg{getMailData()};
  string b = msg.getBodyData();
  ASSERT_TRUE(b.substr(0, 17) == "------=_Part_2143");
}

//----------------------------------------------------------------------------

TEST_F(EmailFixture, HeaderParser)
{
  Message msg{getMailData()};

  const Header* hdr = msg.getHeaderPtr();
  ASSERT_TRUE(hdr->getFieldCount() == 27);

  ASSERT_FALSE(hdr->hasField("dskfjsdklf"));
  ASSERT_TRUE(hdr->hasField("fRoM"));

  auto f = hdr->getRawFieldBody("received");
  ASSERT_EQ(4, f.size());

  f = hdr->getRawFieldBody("sdfsdfsdf");
  ASSERT_EQ(0, f.size());

  f = hdr->getRawFieldBody("mime-version");
  ASSERT_EQ(1, f.size());
  ASSERT_EQ("1.0", f[0]);

  ASSERT_EQ("", hdr->getRawFieldBody_Simple("sdkfskdf"));
  ASSERT_EQ("2016.07.21-54.240.0.6", hdr->getRawFieldBody_Simple("X-SES-Outgoing"));
}

//----------------------------------------------------------------------------

TEST_F(EmailFixture, HeaderComments)
{
  vector<pair<string,string>> raw2parsed{
    {"no comments at all", "no comments at all"},
    {"only one (simple comment) here", "only one  here"},
    {"(only comment)", ""},
    {"(comment) at the start", " at the start"},
    {"comment at (the end)", "comment at "},
    {"", ""},
    {"this is no comment bracket: \\(", "this is no comment bracket: \\("},
    {"test of ((nested) comments) xyz (()) ab", "test of  xyz  ab"},
    {"((dfskdfhs) dsfsdkf) xyz", " xyz"},
    {"xyz((dfskdfhs) dsfsdkf)", "xyz"},
  };

  for (auto& r2p : raw2parsed)
  {
    string dummyMsg = "fname: " + r2p.first;
    dummyMsg += "\r\n\r\nDummyMessageBody";

    Message msg{dummyMsg};

    const Header* hdr = msg.getHeaderPtr();
    ASSERT_TRUE(hdr->getFieldCount() == 1);

    ASSERT_TRUE(hdr->hasField("fname"));
    string bRaw = hdr->getRawFieldBody_Simple("fname");
    string bClean = hdr->getFieldBody_Simple("fname");
    ASSERT_EQ(r2p.first, bRaw);
    ASSERT_EQ(r2p.second, bClean);

    /*HeaderField hf{"dummy", r2p.first};
    string raw = hf.getRawBody();
    string clean = hf.getBody();
    ASSERT_EQ(r2p.first, raw);
    ASSERT_EQ(r2p.second, clean);*/

  }

  // test malformed headers
  vector<string> badHeaders {
    {"closing bracket (missing here"},
    {"("},
    {")"},
    {"((abc)"},
    {"((abc)\\)"},
  };
  for (const string& s : badHeaders)
  {
    string dummyMsg = "fname: " + s;
    dummyMsg += "\r\n\r\nDummyMessageBody";

    ASSERT_THROW(Message{dummyMsg}, MalformedMessage);
  }
}

//----------------------------------------------------------------------------

