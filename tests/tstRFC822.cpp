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

