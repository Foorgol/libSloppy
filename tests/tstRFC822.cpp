#include <iostream>

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

  Header* hdr = msg.getHeaderPtr();
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

    Header* hdr = msg.getHeaderPtr();
    ASSERT_TRUE(hdr->getFieldCount() == 1);

    ASSERT_TRUE(hdr->hasField("fname"));
    string bRaw = hdr->getRawFieldBody_Simple("fname");
    string bClean = hdr->getFieldBody_Simple("fname");
    ASSERT_EQ(r2p.first, bRaw);
    ASSERT_EQ(r2p.second, bClean);
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

    ASSERT_THROW(Message{dummyMsg}, MalformedHeader);
  }
}

//----------------------------------------------------------------------------

