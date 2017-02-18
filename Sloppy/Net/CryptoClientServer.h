/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2017  Volker Knollmann
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
    class CryptoClientServer
    {
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


    public:
      class CryptoServer : public AbstractWorker
      {
      public:
        CryptoServer(PubKey& _pk, SecKey& _sk, int _fd, sockaddr_in sa)
          :AbstractWorker{_fd}, pk{PubKey::asCopy(_pk)}, sk{SecKey::asCopy(_sk)},
           sodium{SodiumLib::getInstance()}, challengeForClient{ChallengeSize},
           dhEx{false}, handshakeComplete{false}
        {
          sk.setAccess(SodiumSecureMemAccess::NoAccess);
        }

        virtual void doTheWork() override final;

        // this one should be overridden if client
        // identities shall be checked
        virtual bool isClientAcceptable(const PubKey& k) const { return true; }

        PubKey getPublicKey() const { return PubKey::asCopy(pk); }

        bool isAuthenticated() const { return handshakeComplete; }

      private:
        SodiumLib* sodium;
        SodiumLib::AsymCrypto_PublicKey pk;
        SodiumLib::AsymCrypto_SecretKey sk;

        ManagedBuffer challengeForClient;
        PubKey clientPubKey;

        AsymNonce asymNonce;

        SymNonce symNonce;
        SymKey sessionKey;

        DiffieHellmannExchanger dhEx;

        bool handshakeComplete;

        bool authStep1();
        bool authStep2();
        bool authStep3();

        bool doAuthProcess();
      };

      //----------------------------------------------------------------------------

      class CryptoClient : public BasicTcpClient
      {
      public:
        CryptoClient(PubKey& _pk, SecKey& _sk, const string& _srvName, int _port)
          :BasicTcpClient{_srvName, _port}, pk{PubKey::asCopy(_pk)},
            sk{SecKey::asCopy(_sk)}, sodium{SodiumLib::getInstance()},
            hasExpectedServerKey{false}, challengeFromServer{},
            dhEx{true}, handshakeComplete{false}
        {
          sk.setAccess(SodiumSecureMemAccess::NoAccess);

          // invalidate the placeholder for the server's public key
          bzero(expectedServerKey.get(), expectedServerKey.getSize());
          bzero(srvPubKey.get(), srvPubKey.getSize());
        }

        virtual void doTheWork() override;

        void setExpectedServerKey(const PubKey& srvPubKey);

        bool cmpServerKeys(const PubKey& srvPubKey) const;

        // this one should be overriden by derived classes if they
        // want to perform more sophisticated checks
        virtual bool isServerAcceptable(const PubKey& k) const
        {
          return cmpServerKeys(k);
        }

        bool isAuthenticated() const { return handshakeComplete; }

      private:
        SodiumLib* sodium;
        PubKey pk;
        SecKey sk;
        PubKey expectedServerKey;
        bool hasExpectedServerKey;

        PubKey srvPubKey;
        ManagedBuffer challengeFromServer;
        AsymNonce asymNonce;

        SymNonce symNonce;
        SymKey sessionKey;

        DiffieHellmannExchanger dhEx;

        bool handshakeComplete;

        bool doAuthProcess();
        bool authStep1();
        bool authStep2();
        bool authStep3();
      };

    private:
      static const string MagicPhrase;
      static constexpr size_t AuthStepTimeout_ms = 1000;
      static constexpr size_t ChallengeSize = 32;
    };
  }
}
#endif
