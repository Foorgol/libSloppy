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

#pragma once

#include <string>
#include <tuple>

#include "../json.hpp"
#include "../DateTime/DateAndTime.h"
#include "Sodium.h"
#include "../BasicException.h"

namespace Sloppy
{
  namespace MiniCert
  {
    /** \brief The current version of the MiniCert file formats */
    static constexpr uint8_t MiniCertVersion = 0;

    /** \brief An enum with types for exported data blocks
     */
    enum class MiniCertDataType : uint8_t
    {
      CertSignRequest,
      SignedCert,

      Invalid = 255
    };

    /** \brief Safe conversion between byte and MiniCertDataType
     *
     * A simple static_cast is undefined for invalid input integers. Since
     * have to assume malicious input, we should not rely on static_cast here.
     *
     * \returns a MiniCertDataType enum that matches the input byte value or "Invalid"
     */
    MiniCertDataType byte2MiniCertDataType(uint8_t b);

    /** \brief Error codes concerning the processing of MiniCerts
     */
    enum class MiniCertError
    {
      Okay,   ///< no error
      BadEncoding,   ///< error when decoding BASE64
      BadSignature,   ///< a signature was invalid
      BadFormat,   ///< provided data was invalid or empty
      BadKey,   ///< a provided key was empty or otherwise invalid
      BadVersion,   ///< invalid protocol version
    };

    /** \brief Exception that indicates an unsupported protocol version */
    class BadVersionException : public Sloppy::BasicException
    {
    public:
      BadVersionException(const std::string& sender, const std::string& context = std::string{}, const std::string& details = std::string{})
        :Sloppy::BasicException("Bad MiniCert Protocol Version", sender, context, details) {}
    };

    /** \brief Exception that indicates a bad, unparseable data format (e.g., too short or empty) */
    class BadDataFormatException : public Sloppy::BasicException
    {
    public:
      BadDataFormatException(const std::string& sender, const std::string& context = std::string{}, const std::string& details = std::string{})
        :Sloppy::BasicException("Bad Data Format", sender, context, details) {}
    };

    /** \brief Exception that indicates a failed signature check */
    class BadSignature : public Sloppy::BasicException
    {
    public:
      BadSignature(const std::string& sender, const std::string& context = std::string{}, const std::string& details = std::string{})
        :Sloppy::BasicException("Bad Signature (e.g., failed signature check for payload)", sender, context, details) {}
    };

    /** \brief Exception that indicates a bad key (e.g. too long, too short, decoding from Base64 failed, ...) */
    class BadKey : public Sloppy::BasicException
    {
    public:
      BadKey(const std::string& sender, const std::string& context = std::string{}, const std::string& details = std::string{})
        :Sloppy::BasicException("Bad Key (e.g., too long, too short, decoding from Base64 failed, ...)", sender, context, details) {}
    };

    /** \brief A common data structure that is used for Certificate Signing Requests
     * and for the certificate itself.
     *
     * \note The `payload` view is only valid as long as the data block that was used
     * for constructing MiniCertFrame instance remains valid.
     */
    class MiniCertFrame
    {
    public:
      /** \brief Default ctor that creates an invalid frame.
       */
      MiniCertFrame();

      /** \brief Standard ctor that takes a `MemView` as input and tries
       * to parse that `MemView` as a MiniCertFrame
       *
       * \note This class only supports version 0 of the protocol
       *
       * \throws BadDataFormatException if the provided data blob was empty or too short
       *
       * \throws BadVersionException if the version tag indicated an unsupported protocol version
       *
       * \throws a possible exception if the type byte in the header cannot be convert into an instance of `MiniCertDataType`
       */
      MiniCertFrame(
          const Sloppy::MemView& data   ///< the input data used for parsing
          );

      /** \returns the version header (currently always "0")
       */
      uint8_t version() const { return m_version; }

      /** \returns the payload type / frame type
       */
      MiniCertDataType type() const { return m_type; }

      /** \returns a `const` reference to the signer's public key
       */
      const Sloppy::Crypto::SodiumLib::AsymSign_PublicKey& signersPubKey() const { return m_signersPubKey; }

      /** \returns a `const` reference to the signature
       */
      const Sloppy::Crypto::SodiumLib::AsymSign_Signature& sig() const { return m_sig; }

      /** \returns a MemView pointing to the payload. NOTE: the payload is *not* owned by this class!
       */
      Sloppy::MemView payload() const { return m_payload; }

      /** \brief Checks the payload against the signature using the contained public key
       *
       * \note Since the actual payload is *not* owned by this class it can be modified outside
       * of this class. A potential modification would of cause invalidate the result of this
       * function.
       *
       * \returns `true` if the signature is valid for the payload; `false` otherwise
       */
      bool isValidSignature() const;

      /** \brief Tries to convert the payload to JSON.
       *
       * \throws BadDataFormatException if the provided payload could not be parsed as
       * valid JSON or if the JSON content is not a "JSON object" (key/value pairs).
       *
       * \returns a JSON object containing the parsed payload
       */
      nlohmann::json payloadAsJSON() const;


    private:
      Crypto::SodiumLib* sodium;
      uint8_t m_version = 255;  ///< the protocol version byte
      MiniCertDataType m_type = MiniCertDataType::Invalid;   ///< the type of the data frame
      Sloppy::Crypto::SodiumLib::AsymSign_PublicKey m_signersPubKey;   ///< the public key that was used for the signature
      Sloppy::Crypto::SodiumLib::AsymSign_Signature m_sig;   ///< the payload signature
      Sloppy::MemView m_payload;   ///< the payload of the MiniCert frame
    };

    /** \brief Creates a new MiniCertFrame from data type, secret signing key and an arbitrary payload.
     *
     * The payload is copied to the target buffer.
     *
     * \throws BadDataFormatException if the provided payload is empty
     *
     * \throws std::invalid_argument if the public key could not be derived from the secret key (unlikely)
     *
     * \returns a heap-allocated memory buffer with the binary MiniCertFrame
     */
    Sloppy::MemArray buildMiniCertFrame(
        MiniCertDataType t,   ///< the data type for the header
        const Sloppy::Crypto::SodiumLib::AsymSign_SecretKey& sk,   ///< the secret key for signing and for deriving the public key
        const Sloppy::MemView& payload   ///< the payload to attach to the header
        );

    /** \brief Creates a new MiniCertFrame from data type, secret signing key and a JSON payload.
     *
     * The payload is copied to the target buffer.
     *
     * \throws BadDataFormatException if the provided payload is empty or not an JSON object
     *
     * \throws std::invalid_argument if the public key could not be derived from the secret key (unlikely)
     *
     * \returns a heap-allocated memory buffer with the binary MiniCertFrame
     */
    Sloppy::MemArray buildMiniCertFrame(
        MiniCertDataType t,   ///< the data type for the header
        const Sloppy::Crypto::SodiumLib::AsymSign_SecretKey& sk,   ///< the secret key for signing and for deriving the public key
        const nlohmann::json& payload   ///< the payload to attach to the header
        );

    /** \brief Internal representation of an OUTGOING Certificate Signing Request
     *
     * Outgoing = from subject to CA
     */
    struct CertSignReqOut
    {
      std::string cn;  ///< the subject's "Common Name" (CN)
      Sloppy::Crypto::SodiumLib::AsymCrypto_PublicKey cryptoPubKey;   ///< the public key used for encryption
      nlohmann::json addSubjectInfo;   ///< any other data that should become part of the subject description

      bool isValid() const;
    };

    /** \brief Internal representation of an INCOMING Certificate Signing Request
     *
     * Incoming = after parsing on the CA side
     */
    struct CertSignReqIn
    {
      std::string cn;  ///< the subject's "Common Name" (CN)
      Sloppy::Crypto::SodiumLib::AsymCrypto_PublicKey cryptoPubKey;   ///< the public key used for encryption
      Sloppy::Crypto::SodiumLib::AsymSign_PublicKey signPubKey;   ///< the public key used for signing cleartext
      Sloppy::DateTime::WallClockTimepoint_secs signatureTimestamp;   ///< the time in UTC when the request was signed by the client
      nlohmann::json addSubjectInfo;   ///< any other data that should become part of the subject description

      bool isValid() const;
    };

    /** \brief Creates an "exportable" (Base64-encoded) Certificate Signing Request
     *
     * Data format specification:
     *  * version indicator, currently "0" (1 Byte)
     *  * the type indicator, taken from MiniCertDataType (1 Byte)
     *  * the subject's public key for signing cleartext
     *  * the subject's signature for the following JSON part
     *  * JSON-string describing the subject
     *
     * The JSON-string will contain at least the following fields:
     *  * cn: the subject's common name
     *  * cpk: the subject's public crypto key in BASE64 (cpk = crypto public key)
     *  * spk: the subject's public signing key in BASE64 (spk = signing public key)
     *  * sts: the time in UTC when the request was signed (sts = signature time stamp)
     *
     * \returns a pair of <error code, string with CSR>; the string is empty in case of errors
     */
    std::pair<MiniCertError, std::string> createCertSigningRequest(
        const CertSignReqOut& csr,   ///< the raw, binary data for the request
        const Crypto::SodiumLib::AsymSign_SecretKey& sk   ///< the subject's secret signing key
        );

    /** \brief Takes an "exported", BASE64-encoded Certificate Signing Request,
     * checks its cryptographical integrity and returns parsed subject description
     *
     * \note For the additional data we always return a JSON instance of type "Object" even if the request
     * was signed without additional subject data. Without additional data the
     * return JSON object instance is simply empty (--> "{}").
     *
     * \returns a pair of <error code, CertSignReqIn>; the CertSignReqIn object contains invalid
     * default data in case of errors
     */
    std::pair<MiniCertError, CertSignReqIn> parseCertSignRequest(
        const std::string& csr   ///< the BASE64-encoded CSR including subject's signature
        );

    /** \brief Signs a CSR and returns a BASE64 encoded certificate
     *
     * Data format specification:
     *  * version tag, currently "0" (1 Byte)
     *  * the type tag, taken from MiniCertDataType (1 Byte)
     *  * the CA's public key
     *  * the CA's signature for the following JSON part
     *  * JSON-string describing the certificate content
     *
     * The JSON-string will contain at least the following fields:
     *  * subject: a JSON-Object with at least the following fields
     *  ** cn: the subject's common name
     *  ** cpk: the subject's public crypto key in BASE64 (cpk = crypto public key)
     *  ** spk: the subject's public signing key in BASE64 (spk = signing public key)
     *  * meta: a JSON-Object with the following fields:
     *  ** vf: "valid from" in UTC seconds
     *  ** vu: "valid until" in UTC seconds
     *  ** ca: the common name of the CA that signed the request
     *  ** sts: the time in UTC when the request was signed (sts = signature time stamp)
     */
    std::pair<MiniCertError, std::string> signCertSignRequest(
        const CertSignReqIn& csr,  ///< the CSR to sign
        const std::string& caName,   ///< the CN of the CA used for signing
        const Crypto::SodiumLib::AsymSign_SecretKey& caKey,   ///< the CA's secret key
        const Sloppy::DateTime::TimeRange_secs& validityRange  ///< the certificate's validity period (may not be an open range!)
        );


    /** \brief A MiniCert that is constructed from a signed, Base64-encoded certificate
     *
     */
    class MiniCert
    {
    public:
      struct Subject
      {
        std::string cn;  ///< the subject's "Common Name" (CN)
        Sloppy::Crypto::SodiumLib::AsymCrypto_PublicKey cryptoPubKey;   ///< the public key used for encryption
        Sloppy::Crypto::SodiumLib::AsymSign_PublicKey signPubKey;   ///< the public key used for signing cleartext
        nlohmann::json addSubjectInfo;   ///< any other data that is part of the subject description
      };

      struct Meta
      {
        std::string caName;   ///< the CA's common name
        Sloppy::Crypto::SodiumLib::AsymSign_PublicKey caPubKey;   ///< the public key used for signing the cert
        Sloppy::DateTime::WallClockTimepoint_secs validFrom;   ///< the validity start for the resulting cert
        Sloppy::DateTime::WallClockTimepoint_secs validUntil; ///< the validity end for the resulting cert
        Sloppy::DateTime::WallClockTimepoint_secs sigTime; ///< the time when the cert was signed
      };

      /** \brief Standard ctor that constructs a certificate from a Base64-encoded string
       *
       * \throws std::invalid_argument if the provided string could not be parsed
       *
       * \throws SodiumConversionError if the decoding of the Base64 string failed
       *
       */
      explicit MiniCert(
          const std::string& certB64   ///< Base64 encoded certificate data as returned by `signCertSignRequest`
          );

      /** \returns the subject's common name
       */
      std::string cn() const { return subject.cn; }

      /** \returns a reference to subject's common name
       */
      const std::string& cnRef() const { return subject.cn; }

      /** \returns a copy of the subject's public crypto key
       */
      Sloppy::Crypto::SodiumLib::AsymCrypto_PublicKey cryptoPubKey() const { return subject.cryptoPubKey; }

      /** \returns a reference to the subject's public crypto key
       */
      const Sloppy::Crypto::SodiumLib::AsymCrypto_PublicKey& cryptoPubKeyRef() const { return subject.cryptoPubKey; }

      /** \returns a copy of the subject's public signing key
       */
      Sloppy::Crypto::SodiumLib::AsymSign_PublicKey signPubKey() const { return subject.signPubKey; }

      /** \returns a reference to the subject's public signing key
       */
      const Sloppy::Crypto::SodiumLib::AsymSign_PublicKey& signPubKeyRef() const { return subject.signPubKey; }

      /** \returns a JSON instance (copy) of type "Object" that is either empty or
       * contains any additional subject information that was provided in the certificate
       */
      nlohmann::json addSubjectInfo() const { return subject.addSubjectInfo; }

      /** \returns a JSON instance (reference) of type "Object" that is either empty or
       * contains any additional subject information that was provided in the certificate
       */
      const nlohmann::json& addSubjectInfoRef() const { return subject.addSubjectInfo; }

      /** \returns the name of the signing CA
       */
      std::string caName() const { return meta.caName; }

      /** \returns the name of the signing CA as a reference
       */
      const std::string& caNameRef() const { return meta.caName; }

    private:
      Crypto::SodiumLib* sodium;
      Subject subject;
      Meta meta;
    };

  }
}
