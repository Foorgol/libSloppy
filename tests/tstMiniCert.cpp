/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2018  Volker Knollmann
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
#include <sstream>

#include <gtest/gtest.h>

#include "../Sloppy/Crypto/MiniCert.h"
#include "../Sloppy/Memory.h"

using namespace Sloppy::Crypto;
using namespace Sloppy::MiniCert;

class MiniCertTestFixture : public ::testing::Test
{
protected:
  SodiumLib* sodium;
  SodiumLib::AsymSign_PublicKey spk;
  SodiumLib::AsymSign_SecretKey ssk;
  SodiumLib::AsymCrypto_PublicKey cpk;
  SodiumLib::AsymCrypto_SecretKey csk;

  virtual void SetUp ()
  {
    sodium = SodiumLib::getInstance();
    sodium->genAsymSignKeyPair(spk, ssk);
    sodium->genAsymCryptoKeyPair(cpk, csk);
  }

  //virtual void TearDown ();
};

//----------------------------------------------------------------------------

TEST_F(MiniCertTestFixture, CreateValidCSR)
{
  // prep a CSR
  CertSignReqOut csrOut;
  csrOut.cn = "Volker";
  csrOut.cryptoPubKey.fillFromMemView(cpk.toMemView());

  // sign and export the CSR
  MiniCertError err;
  string b64;
  tie(err, b64) = Sloppy::MiniCert::createCertSigningRequest(csrOut, ssk);
  ASSERT_EQ(MiniCertError::Okay, err);

  // decode and check the resulting CSR
  string csrPlain = sodium->base642Bin(b64);
  Sloppy::MemView v{csrPlain};
  ASSERT_TRUE(csrPlain.length() > (2 + crypto_sign_PUBLICKEYBYTES + crypto_sign_BYTES));
  ASSERT_EQ(0, csrPlain[0]);  // version tag
  ASSERT_EQ(static_cast<uint8_t>(MiniCertDataType::CertSignRequest), csrPlain[1]);  // type tag
  Sloppy::MemView pkView = v.slice_byCount(2, crypto_sign_PUBLICKEYBYTES);
  ASSERT_TRUE(sodium->memcmp(spk.toMemView(), pkView));  // embedded public signing key
  Sloppy::MemView sigView = v.slice_byCount(2 + crypto_sign_PUBLICKEYBYTES, crypto_sign_BYTES);
  SodiumLib::AsymSign_Signature sig;
  sig.fillFromMemView(sigView);
  Sloppy::MemView jsonView{v};
  jsonView.chopLeft(2 + crypto_sign_PUBLICKEYBYTES + crypto_sign_BYTES);
  ASSERT_TRUE(sodium->sign_verify_detached(jsonView, sig, spk));

  // check the JSON data that describes the subject
  string jsonString{jsonView.to_charPtr(), jsonView.size()};
  nlohmann::json j = nlohmann::json::parse(jsonString);
  ASSERT_TRUE(j.is_object());
  ASSERT_EQ("Volker", j["cn"]);
  time_t stsRaw = j["sts"];
  Sloppy::DateTime::UTCTimestamp now;
  ASSERT_EQ(now.getRawTime(), stsRaw);
  string rawPubKey = j["spk"];
  rawPubKey = sodium->base642Bin(rawPubKey);
  SodiumLib::AsymSign_PublicKey _spk;
  _spk.fillFromString(rawPubKey);
  ASSERT_TRUE(sodium->memcmp(spk.toMemView(), _spk.toMemView()));
  rawPubKey = j["cpk"];
  rawPubKey = sodium->base642Bin(rawPubKey);
  SodiumLib::AsymCrypto_PublicKey _cpk;
  _cpk.fillFromString(rawPubKey);
  ASSERT_TRUE(sodium->memcmp(cpk.toMemView(), _cpk.toMemView()));
}

//----------------------------------------------------------------------------

TEST_F(MiniCertTestFixture, CreateValidCSRWithAdditionalData)
{
  // prep a CSR
  CertSignReqOut csrOut;
  csrOut.cn = "Volker";
  csrOut.cryptoPubKey.fillFromMemView(cpk.toMemView());
  csrOut.addSubjectInfo = nlohmann::json::object({{"x", 42}, {"y", "abc"}});

  // sign and export the CSR
  MiniCertError err;
  string b64;
  tie(err, b64) = Sloppy::MiniCert::createCertSigningRequest(csrOut, ssk);
  ASSERT_EQ(MiniCertError::Okay, err);

  // decode and check the resulting CSR
  string csrPlain = sodium->base642Bin(b64);
  Sloppy::MemView v{csrPlain};
  ASSERT_TRUE(csrPlain.length() > (2 + crypto_sign_PUBLICKEYBYTES + crypto_sign_BYTES));
  ASSERT_EQ(0, csrPlain[0]);  // version tag
  ASSERT_EQ(static_cast<uint8_t>(MiniCertDataType::CertSignRequest), csrPlain[1]);  // type tag
  Sloppy::MemView pkView = v.slice_byCount(2, crypto_sign_PUBLICKEYBYTES);
  ASSERT_TRUE(sodium->memcmp(spk.toMemView(), pkView));  // embedded public signing key
  Sloppy::MemView sigView = v.slice_byCount(2 + crypto_sign_PUBLICKEYBYTES, crypto_sign_BYTES);
  SodiumLib::AsymSign_Signature sig;
  sig.fillFromMemView(sigView);
  Sloppy::MemView jsonView{v};
  jsonView.chopLeft(2 + crypto_sign_PUBLICKEYBYTES + crypto_sign_BYTES);
  ASSERT_TRUE(sodium->sign_verify_detached(jsonView, sig, spk));

  // check the JSON data that describes the subject
  string jsonString{jsonView.to_charPtr(), jsonView.size()};
  nlohmann::json j = nlohmann::json::parse(jsonString);
  ASSERT_TRUE(j.is_object());
  ASSERT_EQ("Volker", j["cn"]);
  ASSERT_EQ(42, j["x"]);
  ASSERT_EQ("abc", j["y"]);
}

//----------------------------------------------------------------------------

TEST_F(MiniCertTestFixture, HandleInvalidCSR)
{
  // prep an invalid CSR
  CertSignReqOut csrOut;  // CN is empty
  csrOut.cryptoPubKey.fillFromMemView(cpk.toMemView());

  // try to sign and export the CSR
  MiniCertError err;
  string b64;
  tie(err, b64) = Sloppy::MiniCert::createCertSigningRequest(csrOut, ssk);
  ASSERT_EQ(MiniCertError::BadFormat, err);
  ASSERT_EQ(0, b64.size());

  // try to overwrite the CN with the additional data
  csrOut.cn = "Dummy";
  csrOut.addSubjectInfo = {{"cn", "xyz"}};
  tie(err, b64) = Sloppy::MiniCert::createCertSigningRequest(csrOut, ssk);
  ASSERT_EQ(MiniCertError::BadFormat, err);
  ASSERT_EQ(0, b64.size());

  // invalid additional json (not of type "Object")
  csrOut.addSubjectInfo = {45,67,78};
  tie(err, b64) = Sloppy::MiniCert::createCertSigningRequest(csrOut, ssk);
  ASSERT_EQ(MiniCertError::BadFormat, err);
  ASSERT_EQ(0, b64.size());

  // empty public crypto key
  csrOut.cryptoPubKey.releaseMemory();
  csrOut.addSubjectInfo = {{"valid", "content"}};
  tie(err, b64) = Sloppy::MiniCert::createCertSigningRequest(csrOut, ssk);
  ASSERT_EQ(MiniCertError::BadFormat, err);
  ASSERT_EQ(0, b64.size());

  // empty signing key
  csrOut.cryptoPubKey = SodiumLib::AsymCrypto_PublicKey{SodiumKeyInitStyle::Random};
  ssk.releaseMemory();
  ASSERT_TRUE(ssk.empty());
  tie(err, b64) = Sloppy::MiniCert::createCertSigningRequest(csrOut, ssk);
  ASSERT_EQ(MiniCertError::BadKey, err);
  ASSERT_EQ(0, b64.size());
}

//----------------------------------------------------------------------------

