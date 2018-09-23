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

  string json2SignedCSR(const nlohmann::json& j)
  {
    string s = j.dump();
    Sloppy::MemView v{s};
    SodiumLib::AsymSign_Signature sig = sodium->sign_detached(v, ssk);
    Sloppy::MemArray result{2 + spk.size() + sig.size() + s.size()};
    result[0] = 0;
    result[1] = 0;
    result.copyOver(spk.toMemView(), 2);
    result.copyOver(sig.toMemView(), 2 + spk.size());
    result.copyOver(v, 2 + sig.size() + spk.size());
    Sloppy::MemArray outB64 = sodium->bin2Base64(result.view());
    return string{outB64.to_charPtr(), outB64.size()};
  }
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

TEST_F(MiniCertTestFixture, ParseValidCSR)
{
  // prep and sign a CSR with additional data
  CertSignReqOut csrOut;
  csrOut.cn = "Volker";
  csrOut.cryptoPubKey.fillFromMemView(cpk.toMemView());
  csrOut.addSubjectInfo = nlohmann::json::object({{"x", 42}, {"y", "abc"}});
  MiniCertError err;
  string csrExport;
  tie(err, csrExport) = Sloppy::MiniCert::createCertSigningRequest(csrOut, ssk);
  ASSERT_EQ(MiniCertError::Okay, err);

  // parse the CSR
  CertSignReqIn csrIn;
  tie(err, csrIn) = Sloppy::MiniCert::parseCertSignRequest(csrExport);
  ASSERT_EQ(MiniCertError::Okay, err);
  ASSERT_EQ("Volker", csrIn.cn);
  ASSERT_TRUE(sodium->memcmp(csrIn.cryptoPubKey.toMemView(), cpk.toMemView()));
  ASSERT_TRUE(sodium->memcmp(csrIn.signPubKey.toMemView(), spk.toMemView()));
  ASSERT_EQ(Sloppy::DateTime::UTCTimestamp{}, csrIn.signatureTimestamp);
  ASSERT_EQ(2, csrIn.addSubjectInfo.size());
  ASSERT_EQ(42, csrIn.addSubjectInfo["x"]);
  ASSERT_EQ("abc", csrIn.addSubjectInfo["y"]);

  // prep and sign a CSR without additional data
  csrOut.addSubjectInfo = nlohmann::json{};
  tie(err, csrExport) = Sloppy::MiniCert::createCertSigningRequest(csrOut, ssk);
  ASSERT_EQ(MiniCertError::Okay, err);

  // parse the CSR
  tie(err, csrIn) = Sloppy::MiniCert::parseCertSignRequest(csrExport);
  ASSERT_EQ(MiniCertError::Okay, err);
  ASSERT_EQ("Volker", csrIn.cn);
  ASSERT_TRUE(sodium->memcmp(csrIn.cryptoPubKey.toMemView(), cpk.toMemView()));
  ASSERT_TRUE(sodium->memcmp(csrIn.signPubKey.toMemView(), spk.toMemView()));
  ASSERT_EQ(Sloppy::DateTime::UTCTimestamp{}, csrIn.signatureTimestamp);
  ASSERT_TRUE(csrIn.addSubjectInfo.is_object());
  ASSERT_EQ(0, csrIn.addSubjectInfo.size());
}

//----------------------------------------------------------------------------

TEST_F(MiniCertTestFixture, ParseInvalidCSR)
{
  MiniCertError err;
  CertSignReqIn csrIn;

  // empty input
  string s{};
  tie(err, csrIn) = Sloppy::MiniCert::parseCertSignRequest(s);
  ASSERT_EQ(MiniCertError::BadFormat, err);

  // invalid BASE64
  s = "skjfskfdh";
  tie(err, csrIn) = Sloppy::MiniCert::parseCertSignRequest(s);
  ASSERT_EQ(MiniCertError::BadEncoding, err);

  // not enough data
  s = sodium->bin2Base64("NotEnough");
  tie(err, csrIn) = Sloppy::MiniCert::parseCertSignRequest(s);
  ASSERT_EQ(MiniCertError::BadFormat, err);

  // create a valid request
  CertSignReqOut csrOut;
  csrOut.cn = "Volker";
  csrOut.cryptoPubKey.fillFromMemView(cpk.toMemView());
  string csrExport;
  tie(err, csrExport) = Sloppy::MiniCert::createCertSigningRequest(csrOut, ssk);
  ASSERT_EQ(MiniCertError::Okay, err);
  string csrRaw = sodium->base642Bin(csrExport);

  // fake the version tag
  csrRaw[0] = 1;
  csrExport = sodium->bin2Base64(csrRaw);
  tie(err, csrIn) = Sloppy::MiniCert::parseCertSignRequest(csrExport);
  ASSERT_EQ(MiniCertError::BadVersion, err);
  csrRaw[0] = 0;

  // fake the type tag
  csrRaw[1] = 42;
  csrExport = sodium->bin2Base64(csrRaw);
  tie(err, csrIn) = Sloppy::MiniCert::parseCertSignRequest(csrExport);
  ASSERT_EQ(MiniCertError::BadFormat, err);
  csrRaw[1] = 0;

  // invalidate the signature by fiddling with
  // the fourth signature byte
  size_t idx = 2 + spk.size() + 3;
  csrRaw[idx] = csrRaw[idx] + 1;
  csrExport = sodium->bin2Base64(csrRaw);
  tie(err, csrIn) = Sloppy::MiniCert::parseCertSignRequest(csrExport);
  ASSERT_EQ(MiniCertError::BadSignature, err);
  csrRaw[idx] = csrRaw[idx] - 1;

  // prep some invalid JSON but first ensure
  // that our little CSR generator works correctly
  nlohmann::json j = {{"cn", "xyz"}};
  Sloppy::MemArray tmp = sodium->bin2Base64(cpk.toMemView());
  j["cpk"] = string{tmp.to_charPtr(), tmp.size()};
  tmp = sodium->bin2Base64(spk.toMemView());
  j["spk"] = string{tmp.to_charPtr(), tmp.size()};
  j["sts"] = Sloppy::DateTime::UTCTimestamp{}.getRawTime();
  csrExport = json2SignedCSR(j);
  tie(err, csrIn) = Sloppy::MiniCert::parseCertSignRequest(csrExport);
  ASSERT_EQ(MiniCertError::Okay, err);
  ASSERT_EQ("xyz", csrIn.cn);

  // okay, the generator works

  // JSON without cn
  ASSERT_EQ(1, j.erase("cn"));
  csrExport = json2SignedCSR(j);
  tie(err, csrIn) = Sloppy::MiniCert::parseCertSignRequest(csrExport);
  ASSERT_EQ(MiniCertError::BadFormat, err);

  // JSON with empty cn
  j["cn"] = "";
  csrExport = json2SignedCSR(j);
  tie(err, csrIn) = Sloppy::MiniCert::parseCertSignRequest(csrExport);
  ASSERT_EQ(MiniCertError::BadFormat, err);

  j["cn"] = "xyz";

  // JSON without public key or with invalid public key
  for (const string& k : {"spk", "cpk"})
  {
    string old = j[k];
    ASSERT_EQ(1, j.erase(k));
    csrExport = json2SignedCSR(j);
    tie(err, csrIn) = Sloppy::MiniCert::parseCertSignRequest(csrExport);
    ASSERT_EQ(MiniCertError::BadFormat, err);

    for (const string& badKey : {
         "",  // empty
         "ddjkfg",  // invalid encoding
         "c2hvcnQK"  // valid encoding but too short
         })
    {
      j[k] = badKey;
      csrExport = json2SignedCSR(j);
      tie(err, csrIn) = Sloppy::MiniCert::parseCertSignRequest(csrExport);
      ASSERT_EQ(MiniCertError::BadFormat, err);
    }

    j[k] = old;
  }

  // signature timestamp in the future
  j["sts"] = Sloppy::DateTime::UTCTimestamp{}.getRawTime() + 10;
  csrExport = json2SignedCSR(j);
  tie(err, csrIn) = Sloppy::MiniCert::parseCertSignRequest(csrExport);
  ASSERT_EQ(MiniCertError::BadFormat, err);

  // restore everything and make sure that we're back at a valid request
  j["sts"] = Sloppy::DateTime::UTCTimestamp{}.getRawTime() - 10;
  csrExport = json2SignedCSR(j);
  tie(err, csrIn) = Sloppy::MiniCert::parseCertSignRequest(csrExport);
  ASSERT_EQ(MiniCertError::Okay, err);
}
