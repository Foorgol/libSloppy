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

    MiniCertDataType byte2MiniCertDataType(uint8_t b)
    {
      switch (b)
      {
      case static_cast<uint8_t>(MiniCertDataType::CertSignRequest):
        return MiniCertDataType::CertSignRequest;

      case static_cast<uint8_t>(MiniCertDataType::SignedCert):
        return MiniCertDataType::SignedCert;

      default:
        return MiniCertDataType::Invalid;
      }

      return MiniCertDataType::Invalid;  // should never be reached
    }

    //----------------------------------------------------------------------------

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

    bool CertSignReqIn::isValid() const
    {
      if (cn.empty() || cryptoPubKey.empty() || signPubKey.empty()) return false;
      if (signatureTimestamp > Sloppy::DateTime::UTCTimestamp{}) return false;
      if (!(addSubjectInfo.is_object())) return false;

      // additional info may not overwrite CN
      if (addSubjectInfo.find("cn") != addSubjectInfo.end()) return false;

      return true;
    }

    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    MiniCertFrame::MiniCertFrame()
      :sodium{Sloppy::Crypto::SodiumLib::getInstance()},
        m_signersPubKey{Sloppy::Crypto::SodiumKeyInitStyle::Zeros},
        m_sig{Sloppy::Crypto::SodiumKeyInitStyle::Zeros},
        m_payload{}
    {
    }

    //----------------------------------------------------------------------------

    MiniCertFrame::MiniCertFrame(const MemView& data)
      :MiniCertFrame()
    {
      // check the minimum data length
      size_t minLen =
          1 + // version
          1 + // file type indicator
          crypto_sign_PUBLICKEYBYTES + // public key for signing
          crypto_sign_BYTES + // signature
          2;   // shortest valid JSON string "{}"
      if (data.size() < minLen)
      {
        throw BadDataFormatException("MiniCertFrame ctor");
      }

      m_version = data[0];
      if (m_version != MiniCertVersion)
      {
        throw BadVersionException("MiniCertFrame ctor");
      }

      // the following might throw an exception, depending on
      // the compiler, if the type flag is invalid.
      m_type = byte2MiniCertDataType(data[1]);

      // extract the binary public key
      Sloppy::MemView pkView = data.slice_byCount(2, m_signersPubKey.size());
      m_signersPubKey.fillFromMemView(pkView);

      // extract the signature
      Sloppy::MemView sigView = data.slice_byCount(2 + m_signersPubKey.size(), m_sig.size());
      m_sig.fillFromMemView(sigView);

      // extract the payload part
      m_payload = data;
      m_payload.chopLeft(2 + m_signersPubKey.size() + m_sig.size());
    }

    //----------------------------------------------------------------------------

    bool MiniCertFrame::isValidSignature() const
    {
      return sodium->sign_verify_detached(m_payload, m_sig, m_signersPubKey);
    }

    //----------------------------------------------------------------------------

    nlohmann::json MiniCertFrame::payloadAsJSON() const
    {
      const string jsonString{m_payload.to_charPtr(), m_payload.size()};
      nlohmann::json j;
      try
      {
        j = nlohmann::json::parse(jsonString);
      }
      catch (...)
      {
        throw BadDataFormatException("MiniCertFrame", "payload conversion to JSON failed");
      }
      if (!(j.is_object()))
      {
        throw BadDataFormatException("MiniCertFrame", "payload conversion to JSON failed");
      }

      return j;
    }

    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    MemArray buildMiniCertFrame(MiniCertDataType t, const Crypto::SodiumLib::AsymSign_SecretKey& sk, const MemView& payload)
    {
      Sloppy::Crypto::SodiumLib* sodium = Sloppy::Crypto::SodiumLib::getInstance();

      if (payload.empty())
      {
        throw BadDataFormatException("buildMiniCertFrame", "", "called with empty payload");
      }

      size_t len =
          1 + // version
          1 + // file type indicator
          crypto_sign_PUBLICKEYBYTES + // public key for signing
          crypto_sign_BYTES + // signature
          payload.size();
      MemArray result{len};

      result[0] = MiniCertVersion;
      result[1] = static_cast<uint8_t>(t);

      // generate the public key from the secret key
      Sloppy::Crypto::SodiumLib::AsymSign_PublicKey pk;
      if (!(sodium->genPublicSignKeyFromSecretKey(sk, pk)))
      {
        throw std::invalid_argument("buildMiniCertFrame: could not compute public key from secret key!");
      }

      // add the public key to the header
      result.copyOver(pk.toMemView(), 2);

      // calculate and attach the signature
      auto sig = sodium->sign_detached(payload, sk);
      result.copyOver(sig.toMemView(), 2 + pk.size());

      // attach the payload
      result.copyOver(payload, 2 + pk.size() + sig.size());

      // done
      return result;
    }

    //----------------------------------------------------------------------------

    MemArray buildMiniCertFrame(MiniCertDataType t, const Crypto::SodiumLib::AsymSign_SecretKey& sk, const nlohmann::json& payload)
    {
      if (!(payload.is_object()))
      {
        throw BadDataFormatException("buildMiniCertFrame", "", "provided JSON data is not a JSON object");
      }

      string s = payload.dump();
      MemView sView{s};

      return buildMiniCertFrame(t, sk, sView);
    }

    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    pair<MiniCertError, string> createCertSigningRequest(const CertSignReqOut& csr, const Crypto::SodiumLib::AsymSign_SecretKey& sk)
    {
      Sloppy::Crypto::SodiumLib* sodium = Sloppy::Crypto::SodiumLib::getInstance();

      if (!csr.isValid()) return make_pair(MiniCertError::BadFormat, "");
      if (sk.empty()) return make_pair(MiniCertError::BadKey, "");

      // derive the public signing key from the private
      // signing key
      Crypto::SodiumLib::AsymSign_PublicKey spk;
      if (!(sodium->genPublicSignKeyFromSecretKey(sk, spk))) return make_pair(MiniCertError::BadKey, "");

      // prepare the resulting JSON object with all subject data points
      nlohmann::json jOut = csr.addSubjectInfo.is_object() ? csr.addSubjectInfo : nlohmann::json::object();
      Sloppy::DateTime::UTCTimestamp now;
      jOut["sts"] = now.getRawTime();
      jOut["cn"] = csr.cn;
      jOut["cpk"] = csr.cryptoPubKey.toBase64();
      jOut["spk"] = spk.toBase64();

      // prepare the binary CSR
      MemArray csrRaw = buildMiniCertFrame(MiniCertDataType::CertSignRequest, sk, jOut);

      // Base64-encode the CSR
      Sloppy::MemArray b64 = sodium->bin2Base64(csrRaw.view());
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

      Sloppy::MemView bin{binString};

      try {
        MiniCertFrame f{bin};
        if (f.type() != MiniCertDataType::CertSignRequest)
        {
          return make_pair(MiniCertError::BadFormat, CertSignReqIn{});
        }

        if (!(f.isValidSignature()))
        {
          return make_pair(MiniCertError::BadSignature, CertSignReqIn{});
        }

        nlohmann::json j = f.payloadAsJSON();

        // prepare the result object
        CertSignReqIn result;

        // check the mandatory fields
        result.cn = j.value("cn", "");
        time_t sts = j.value("sts", 0);
        string cpkB64 = j.value("cpk", "");
        string spkB64 = j.value("spk", "");
        if (result.cn.empty() || cpkB64.empty() || spkB64.empty() || (sts == 0))
        {
          return make_pair(MiniCertError::BadFormat, CertSignReqIn{});
        }

        result.signatureTimestamp = Sloppy::DateTime::UTCTimestamp{sts};

        // check the plausibility of the timestamp
        if (result.signatureTimestamp > Sloppy::DateTime::UTCTimestamp{})
        {
          return make_pair(MiniCertError::BadFormat, CertSignReqIn{});
        }

        // decode the public keys
        if (!(result.cryptoPubKey.fillFromBase64(cpkB64)))
        {
          return make_pair(MiniCertError::BadFormat, CertSignReqIn{});
        }
        if (!(result.signPubKey.fillFromBase64(spkB64)))
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
      catch (BadVersionException)
      {
        return make_pair(MiniCertError::BadVersion, CertSignReqIn{});
      }
      catch (...)
      {
        return make_pair(MiniCertError::BadFormat, CertSignReqIn{});
      }

      // we should never reach this point
      return make_pair(MiniCertError::BadFormat, CertSignReqIn{});
    }

    //----------------------------------------------------------------------------

    pair<MiniCertError, string> signCertSignRequest(const CertSignReqIn& csr, const string& caName, const Crypto::SodiumLib::AsymSign_SecretKey& caKey, const DateTime::UTCTimestamp& validFrom, const DateTime::UTCTimestamp& validUntil)
    {
      Sloppy::Crypto::SodiumLib* sodium = Sloppy::Crypto::SodiumLib::getInstance();

      // sanity checks
      if (!csr.isValid()) return make_pair(MiniCertError::BadFormat, "");
      if (caName.empty()) return make_pair(MiniCertError::BadFormat, "");
      if (caKey.empty()) return make_pair(MiniCertError::BadKey, "");
      if (validUntil < validFrom) return make_pair(MiniCertError::BadFormat, "");
      if (validUntil < Sloppy::DateTime::UTCTimestamp{}) return make_pair(MiniCertError::BadFormat, ""); // create no certs that are expired

      // prepare the subject's JSON data
      nlohmann::json subject = csr.addSubjectInfo;  // this is guaranteed to be a (possibly empty) JSON instance of type "Object"
      subject["cn"] = csr.cn;
      subject["cpk"] = csr.cryptoPubKey.toBase64();
      subject["spk"] = csr.signPubKey.toBase64();

      // prepare the meta data
      nlohmann::json meta = nlohmann::json::object();
      meta["ca"] = caName;
      meta["vf"] = validFrom.getRawTime();
      meta["vu"] = validUntil.getRawTime();
      meta["sts"] = Sloppy::DateTime::UTCTimestamp{}.getRawTime();

      // combine "subject" and "meta" and generate the
      // resulting json string
      nlohmann::json combined = nlohmann::json::object();
      combined["subject"] = subject;
      combined["meta"] = meta;

      // create the raw cert frame
      MemArray rawCert = buildMiniCertFrame(MiniCertDataType::SignedCert, caKey, combined);

      // return the cert in Base64
      Sloppy::MemArray tmp = sodium->bin2Base64(rawCert.view());
      return make_pair(MiniCertError::Okay, string{tmp.to_charPtr(), tmp.size()});
    }

    //----------------------------------------------------------------------------

    MiniCert::MiniCert(const string& certB64)
      :sodium{Crypto::SodiumLib::getInstance()}
    {
      if (certB64.empty())
      {
        throw BadDataFormatException("MiniCert ctor", "", "called with empty string");
      }

      // decode the Base64 and don't catch any exception
      // so that the caller has a chance to react to them
      string binString = sodium->base642Bin(certB64, "\r\n\t");  // ignore line breaks, tabs, etc.

      // try to parse the binare data as a MiniCertFrame
      //
      // don't catch any exceptions; leave their handling to the caller
      MiniCertFrame f{Sloppy::MemView{binString}};


      // check the type tag
      if (f.type() != MiniCertDataType::SignedCert)
      {
        throw BadDataFormatException("MiniCert ctor", "", "Invalid frame type");
      }

      // check the signature
      if (!(f.isValidSignature()))
      {
        throw BadSignature("MiniCert ctor", "", "Signature check for payload failed");
      }

      // extract the CA's public key
      meta.caPubKey.fillFromMemView(f.signersPubKey().toMemView());

      // parse the JSON data
      nlohmann::json j = f.payloadAsJSON();

      //
      // copy data from JSON to the internal member variables
      //

      if ((j.find("subject") == j.end()) || (j.find("meta") == j.end()))
      {
        throw BadDataFormatException("MiniCert ctor", "", "certificate contains invalid JSON (bad structure)");
      }
      nlohmann::json jSub = j["subject"];
      nlohmann::json jMeta = j["meta"];

      // subject data
      subject.cn = jSub.value("cn", "");
      string cpkB64 = jSub.value("cpk", "");
      string spkB64 = jSub.value("spk", "");
      if (subject.cn.empty() || cpkB64.empty() || spkB64.empty())
      {
        throw BadDataFormatException("MiniCert ctor", "", "certificate contains invalid JSON (bad structure)");
      }
      if (!(subject.cryptoPubKey.fillFromBase64(cpkB64)))
      {
        throw BadKey("MiniCert ctor", "", "invalid public crypto key");
      }
      if (!(subject.signPubKey.fillFromBase64(spkB64)))
      {
        throw BadKey("MiniCert ctor", "", "invalid public signing key");
      }
      for (const string& k : {"cn", "cpk", "spk"})
      {
        jSub.erase(k);
      }
      subject.addSubjectInfo = jSub;  // what remains after deleting the mandatory keys is additional data

      // meta data
      time_t t = jMeta.value("vf", 0);
      if (t == 0)
      {
        throw BadDataFormatException("MiniCert ctor", "", "invalid time stamp for 'valid from' (vf)");
      }
      meta.validFrom = Sloppy::DateTime::UTCTimestamp{t};
      t = jMeta.value("vu", 0);
      if (t == 0)
      {
        throw BadDataFormatException("MiniCert ctor", "", "invalid time stamp for 'valid until' (vu)");
      }
      meta.validUntil = Sloppy::DateTime::UTCTimestamp{t};
      t = jMeta.value("sts", 0);
      if (t == 0)
      {
        throw BadDataFormatException("MiniCert ctor", "", "invalid signature time stamp (sts)");
      }
      meta.sigTime = Sloppy::DateTime::UTCTimestamp{t};
      if (meta.validFrom > meta.validUntil)
      {
        throw BadDataFormatException("MiniCert ctor", "", "inconsistent validity timestamps");
      }
      meta.caName = jMeta.value("ca", "");
      if (meta.caName.empty())
      {
        throw BadDataFormatException("MiniCert ctor", "", "missing common name (CN) of the signing CA");
      }
    }



  }
}
