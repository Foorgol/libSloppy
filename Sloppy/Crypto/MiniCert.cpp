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

#include "MiniCert.h"

namespace Sloppy
{
  namespace MiniCert
  {

    bool CertSignReqOut::isValid() const
    {
      if (cn.empty()) return false;
      if (cryptoPubKey.empty()) return false;

      // the JSON must be either null or an object
      if ((!addSubjectInfo.is_null()) && (!addSubjectInfo.is_object())) return false;

      // additional info may not overwrite CN
      if (addSubjectInfo.find("cn") != addSubjectInfo.end()) return false;

      return true;
    }

    //----------------------------------------------------------------------------

    pair<MiniCertError, string> createCertSigningRequest(const CertSignReqOut& csr, const Crypto::SodiumLib::AsymSign_SecretKey& sk)
    {
      Sloppy::Crypto::SodiumLib* sodium = Sloppy::Crypto::SodiumLib::getInstance();

      if (!csr.isValid()) return make_pair(MiniCertError::BadFormat, "");
      if (sk.empty()) return make_pair(MiniCertError::BadKey, "");

      // prepare the resulting JSON object with all subject data points
      nlohmann::json jOut = csr.addSubjectInfo.is_object() ? csr.addSubjectInfo : nlohmann::json::object();
      Sloppy::DateTime::UTCTimestamp now;
      jOut["sts"] = now.getRawTime();
      jOut["cn"] = csr.cn;
      MemArray b64 = sodium->bin2Base64(csr.cryptoPubKey.toMemView());
      jOut["cpk"] = string{b64.to_charPtr(), b64.size()};
      Crypto::SodiumLib::AsymSign_PublicKey spk;
      if (!(sodium->genPublicSignKeyFromSecretKey(sk, spk))) return make_pair(MiniCertError::BadKey, "");
      b64 = sodium->bin2Base64(spk.toMemView());
      jOut["spk"] = string{b64.to_charPtr(), b64.size()};
      string subjData = jOut.dump();

      // sign the subject data
      Crypto::SodiumLib::AsymSign_Signature sig = sodium->sign_detached(subjData, sk);

      // create the actual CSR as a binary block
      size_t csrRawSize =
          1 + // version
          1 + // file type indicator
          spk.size() + // public key for signing
          sig.size() + // signature
          subjData.size();   // JSON data block
      MemArray csrRaw{csrRawSize};
      csrRaw[0] = MiniCertVersion;
      csrRaw[1] = static_cast<uint8_t>(MiniCertDataType::CertSignRequest);
      csrRaw.copyOver(spk.toMemView(), 2);
      csrRaw.copyOver(sig.toMemView(), 2 + spk.size());
      MemView v{subjData};
      csrRaw.copyOver(v, 2 + spk.size() + sig.size());

      // Base64-encode the CSR
      b64 = sodium->bin2Base64(csrRaw.view());
      return make_pair(MiniCertError::Okay, string{b64.to_charPtr(), b64.size()});
    }

    //----------------------------------------------------------------------------

    pair<MiniCertError, CertSignReqIn> parseCertSignRequest(string csr)
    {
      Sloppy::Crypto::SodiumLib* sodium = Sloppy::Crypto::SodiumLib::getInstance();

      if (csr.empty()) return make_pair(MiniCertError::BadFormat, CertSignReqIn{});

      // try to decode the BASE64 data
      string binString;
      try
      {
        binString = sodium->base642Bin(csr, "\r\n\t");  // ignore line breaks, tabs, etc.
      }
      catch (...)
      {
        return make_pair(MiniCertError::BadEncoding, CertSignReqIn{});
      }

      // check the minimum data length
      size_t minLen =
          1 + // version
          1 + // file type indicator
          crypto_sign_PUBLICKEYBYTES + // public key for signing
          crypto_sign_BYTES + // signature
          2;   // shortest valid JSON string "{}"
      if (binString.size() < minLen)
      {
        return make_pair(MiniCertError::BadFormat, CertSignReqIn{});
      }

      Sloppy::MemView bin{binString};

      // check the version
      if (bin[0] != MiniCertVersion)
      {
        return make_pair(MiniCertError::BadVersion, CertSignReqIn{});
      }

      // check the type tag
      if (bin[1] != static_cast<uint8_t>(MiniCertDataType::CertSignRequest))
      {
        return make_pair(MiniCertError::BadFormat, CertSignReqIn{});
      }

      // extract the binary public key
      Sloppy::Crypto::SodiumLib::AsymSign_PublicKey pk;
      Sloppy::MemView pkView = bin.slice_byCount(2, pk.size());
      pk.fillFromMemView(pkView);

      // extract the signature
      Sloppy::Crypto::SodiumLib::AsymSign_Signature sig;
      Sloppy::MemView sigView = bin.slice_byCount(2 + pk.size(), sig.size());
      sig.fillFromMemView(sigView);

      // extract the JSON part
      Sloppy::MemView jsonView{bin};
      jsonView.chopLeft(2 + pk.size() + sig.size());

      // check the signature
      if (!(sodium->sign_verify_detached(jsonView, sig, pk)))
      {
        return make_pair(MiniCertError::BadSignature, CertSignReqIn{});
      }

      // parse the JSON data
      const string jsonString{jsonView.to_charPtr(), jsonView.size()};
      nlohmann::json j;
      try
      {
        j = nlohmann::json::parse(jsonString);
      }
      catch (...)
      {
        return make_pair(MiniCertError::BadFormat, CertSignReqIn{});
      }
      if (!(j.is_object()))
      {
        return make_pair(MiniCertError::BadFormat, CertSignReqIn{});
      }

      // check the mandatory fields
      string cn = j.value("cn", "");
      time_t sts = j.value("sts", 0);
      string cpkB64 = j.value("cpk", "");
      string spkB64 = j.value("spk", "");
      if (cn.empty() || cpkB64.empty() || spkB64.empty() || (sts == 0))
      {
        return make_pair(MiniCertError::BadFormat, CertSignReqIn{});
      }

      // prepare the result object
      CertSignReqIn result;
      result.cn = cn;
      result.signatureTimestamp = Sloppy::DateTime::UTCTimestamp{sts};

      // check the plausibility of the timestamp
      if (result.signatureTimestamp > Sloppy::DateTime::UTCTimestamp{})
      {
        return make_pair(MiniCertError::BadFormat, CertSignReqIn{});
      }

      // decode the public keys
      try
      {
        string decoded = sodium->base642Bin(cpkB64);
        if (!(result.cryptoPubKey.fillFromString(decoded)))
        {
          return make_pair(MiniCertError::BadFormat, CertSignReqIn{});
        }
        decoded = sodium->base642Bin(spkB64);
        if (!(result.signPubKey.fillFromString(decoded)))
        {
          return make_pair(MiniCertError::BadFormat, CertSignReqIn{});
        }
      }
      catch (...)
      {
        return make_pair(MiniCertError::BadFormat, CertSignReqIn{});
      }

      // remove the standard fields from the JSON add check if
      // there is any additional data left
      for (const string& k : {"cn", "sts", "cpk", "spk"})
      {
        if (j.erase(k) != 1)
        {
          throw std::runtime_error("parseCertSignRequest: JSON inconsistency");
        }
      }
      if (j.size() > 0)
      {
        result.addSubjectInfo = j;
      } else {
        result.addSubjectInfo = nlohmann::json::object();
      }

      return make_pair(MiniCertError::Okay, result);
    }

  }
}
