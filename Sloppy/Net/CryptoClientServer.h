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

using namespace std;

using namespace Sloppy::Crypto;

namespace Sloppy
{
  namespace Net
  {
    class CryptoClientServer
    {
    public:
      class CryptoServer : public AbstractWorker
      {
      public:
        CryptoServer(SodiumLib::AsymCrypto_PublicKey& _pk, SodiumLib::AsymCrypto_SecretKey& _sk, int _fd, sockaddr_in sa)
          :AbstractWorker{_fd}, pk{SodiumLib::AsymCrypto_PublicKey::asCopy(_pk)},
            sk{SodiumLib::AsymCrypto_SecretKey::asCopy(_sk)}, sodium{SodiumLib::getInstance()} {}

        virtual void doTheWork() override;

      private:
        SodiumLib* sodium;
        SodiumLib::AsymCrypto_PublicKey pk;
        SodiumLib::AsymCrypto_SecretKey sk;

        bool doAuthProcess();
      };

      //----------------------------------------------------------------------------

      class CryptoClient : public BasicTcpClient
      {
      public:
        CryptoClient(SodiumLib::AsymCrypto_PublicKey& _pk, SodiumLib::AsymCrypto_SecretKey& _sk, const string& _srvName, int _port)
          :BasicTcpClient{_srvName, _port}, pk{SodiumLib::AsymCrypto_PublicKey::asCopy(_pk)},
            sk{SodiumLib::AsymCrypto_SecretKey::asCopy(_sk)}, sodium{SodiumLib::getInstance()} {}

        virtual void doTheWork() override;

      private:
        SodiumLib* sodium;
        SodiumLib::AsymCrypto_PublicKey pk;
        SodiumLib::AsymCrypto_SecretKey sk;

        bool doAuthProcess();
      };

    private:
      static const string MagicPhrase;
      static constexpr size_t AuthStepTimeout_ms = 1000;
      static constexpr size_t ChallengeSize = 32;
    };
  }
}
#endif
