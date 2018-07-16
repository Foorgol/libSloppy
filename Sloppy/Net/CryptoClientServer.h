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

#ifndef SLOPPY__CRYPTO_CLIENT_SERVER_H
#define SLOPPY__CRYPTO_CLIENT_SERVER_H

#include <iostream>
#include <atomic>

#include "TcpClientServer.h"
#include "../Crypto/Sodium.h"
#include "../Crypto/Crypto.h"

using namespace std;

using namespace Sloppy::Crypto;

namespace Sloppy
{
  namespace Net
  {
    class CryptoClientServer : public AbstractWorker
    {
    public:
      // the whole concepts relies on sodium's DH function to
      // return shared secrets of the same size we need for
      // the symmetric encryption!
      static_assert(crypto_generichash_BYTES == crypto_secretbox_KEYBYTES, "Waaah, we have issues with libsodium");

      using PubKey = SodiumLib::AsymCrypto_PublicKey;
      using SecKey = SodiumLib::AsymCrypto_SecretKey;
      using AsymNonce = SodiumLib::AsymCrypto_Nonce;
      using AsymMac = SodiumLib::AsymCrypto_Tag;
      using SymKey = SodiumLib::SecretBoxKeyType;
      using SymNonce = SodiumLib::SecretBoxNonceType;

      enum class ResponseReaction
      {
        SendAndContinue,
        SendAndQuit,
        QuitWithoutSending,
        ContinueWithoutSending
      };

      CryptoClientServer(int _fd, const PubKey& _pk, const SecKey& _sk)
        :AbstractWorker{_fd}, pk{PubKey::asCopy(_pk)}, sk{SecKey::asCopy(_sk)},
          sodium{SodiumLib::getInstance()}, challengeForPeer{ChallengeSize},
          challengeFromPeer{ChallengeSize},
          dhEx{true},    // THIS IS JUST A PLACEHOLDER AND NEEDS TO BE RE-SET BY THE CLIENT / SERVER
          handshakeComplete{false}
      {
        sk.setAccess(SodiumSecureMemAccess::NoAccess);
        bzero(peerPubKey.get(), peerPubKey.getSize());
      }

      CryptoClientServer(ManagedSocket s, const PubKey& _pk, const SecKey& _sk)
        :CryptoClientServer{s.releaseDescriptor(), _pk, _sk}{}

      virtual ~CryptoClientServer() {}

      PubKey getPublicKey() const { return PubKey::asCopy(pk); }
      bool isAuthenticated() const { return handshakeComplete; }

      // this one should be overridden if peer identities (client / server)
      // shall be checked
      virtual bool isPeerAcceptable(const PubKey& k) const { return true; }

      bool encryptAndWrite(const ManagedMemory& msg);
      pair<PreemptiveReadResult, ManagedBuffer> readAndDecrypt(size_t timeout_ms);

    protected:
      SodiumLib::AsymCrypto_PublicKey pk;
      SodiumLib::AsymCrypto_SecretKey sk;
      SodiumLib* const sodium;

      ManagedBuffer challengeForPeer;
      ManagedBuffer challengeFromPeer;

      PubKey peerPubKey;

      AsymNonce asymNonce;

      SymNonce symNonce;
      SymKey sessionKey;

      DiffieHellmannExchanger dhEx;

      bool handshakeComplete;

      static const string MagicPhrase;
      static constexpr size_t AuthStepTimeout_ms = 1000;
      static constexpr size_t ChallengeSize = 32;
      static constexpr uint8_t ProtoVersionMajor = 0;
      static constexpr uint8_t ProtoVersionMinor = 1;
      static constexpr uint8_t ProtoVersionPatch = 0;
    };

    //----------------------------------------------------------------------------

    class CryptoServer : public CryptoClientServer
    {
    public:
      CryptoServer(PubKey& _pk, SecKey& _sk, int _fd)
        :CryptoClientServer{_fd, _pk, _sk}
      {
        dhEx = DiffieHellmannExchanger{false};
      }

      virtual ~CryptoServer() {}

      // this one should be overridden by derived classes
      virtual pair<ResponseReaction, ManagedBuffer> handleRequest(const ManagedBuffer& reqData);

      //
      // Other functions / helpers
      //
      virtual void doTheWork() override final;

    private:
      bool authStep1();
      bool authStep2();
      bool authStep3();
      bool doAuthProcess();
    };

    //----------------------------------------------------------------------------

    class CryptoClient : public CryptoClientServer
    {
    public:
      CryptoClient(PubKey& _pk, SecKey& _sk, int _fd)
        :CryptoClientServer{_fd, _pk, _sk},
          hasExpectedServerKey{false}
      {
        // invalidate the placeholder for the server's public key
        bzero(expectedServerKey.get(), expectedServerKey.getSize());
      }

      CryptoClient(PubKey& _pk, SecKey& _sk, const string& _srvName, int _port)
        :CryptoClient{_pk, _sk, getRawConnectedClientSocket(_srvName, _port)} {}

      virtual ~CryptoClient() {}

      void setExpectedServerKey(const PubKey& srvPubKey);

      bool cmpServerKeys(const PubKey& srvPubKey) const;

      bool doAuthProcess();

      // this one should be overriden by derived classes if they
      // want to perform more sophisticated checks
      virtual bool isPeerAcceptable(const PubKey& k) const override
      {
        return cmpServerKeys(k);
      }

    private:
      PubKey expectedServerKey;
      bool hasExpectedServerKey;

      bool authStep1();
      bool authStep2();
      bool authStep3();
    };
  }
}
#endif
