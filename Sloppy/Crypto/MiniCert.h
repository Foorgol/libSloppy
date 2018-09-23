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

#ifndef SLOPPY__MINICERT_H
#define SLOPPY__MINICERT_H

#include <string>
#include <tuple>

#include "../json.hpp"
#include "../DateTime/DateAndTime.h"
#include "Sodium.h"

using namespace std;

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
    };

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

    /** \brief Internal representation of an OUTGOING Certificate Signing Request
     *
     * Outgoing = from subject to CA
     */
    struct CertSignReqOut
    {
      string cn;  ///< the subject's "Common Name" (CN)
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
      string cn;  ///< the subject's "Common Name" (CN)
      Sloppy::Crypto::SodiumLib::AsymCrypto_PublicKey cryptoPubKey;   ///< the public key used for encryption
      Sloppy::Crypto::SodiumLib::AsymSign_PublicKey signPubKey;   ///< the public key used for signing cleartext
      Sloppy::DateTime::UTCTimestamp signatureTimestamp;   ///< the time in UTC when the request was signed by the client
      nlohmann::json addSubjectInfo;   ///< any other data that should become part of the subject description
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
    pair<MiniCertError, string> createCertSigningRequest(
        const CertSignReqOut& csr,   ///< the raw, binary data for the request
        const Crypto::SodiumLib::AsymSign_SecretKey& sk   ///< the subject's secret signing key
        );

    /** \brief Takes an "exportet", BASE64-encoded Certificate Signing Request,
     * checks its cryptographical integrity and returns parsed subject description
     *
     * \note For the additional data we always return a JSON instance of type "Object" even if the request
     * was signed without additional subject data. Without additional data the
     * return JSON object instance is simply empty (--> "{}").
     *
     * \returns a pair of <error code, CertSignReqIn>; the CertSignReqIn object contains invalid
     * default data in case of errors
     */
    pair <MiniCertError, CertSignReqIn> parseCertSignRequest(
        string csr   ///< the BASE64-encoded CSR including subject's signature
        );
  }
}

#endif
