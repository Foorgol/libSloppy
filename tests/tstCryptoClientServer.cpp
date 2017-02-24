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

#include <iostream>
#include <thread>

#include <gtest/gtest.h>

#include "../Sloppy/Net/CryptoClientServer.h"
#include "../Sloppy/Crypto/Sodium.h"
#include "../Sloppy/Crypto/Crypto.h"
#include "../Sloppy/libSloppy.h"

using namespace Sloppy;
using namespace Sloppy::Net;

TEST(CryptoClientServerDemo, HelloWorld)
{
  // prepare a server worker class that waits for a
  // data from the client and responds with a copy of that data
  class SrvWorker : public CryptoClientServer::CryptoServer
  {
  public:
    SrvWorker(SodiumLib::AsymCrypto_PublicKey& _pk, SodiumLib::AsymCrypto_SecretKey& _sk, int _fd, sockaddr_in sa)
      :Net::CryptoClientServer::CryptoServer(_pk, _sk, _fd, sa) {}

    virtual pair<CryptoClientServer::RequestResponse, ManagedBuffer> handleRequest(const SodiumSecureMemory& reqData) override
    {
      ManagedBuffer returnCopy = ManagedBuffer::asCopy(reqData);

      return make_pair(CryptoClientServer::RequestResponse::SendAndContinue, std::move(returnCopy));
    }
  };

  // prepare a simple worker factory
  class SrvWorkerFactory : public AbstractWorkerFactory
  {
  public:
    SrvWorkerFactory()
      :AbstractWorkerFactory(), sodium{SodiumLib::getInstance()}
    {
      sodium->genAsymCryptoKeyPair(pk, sk);
    }

    SodiumLib::AsymCrypto_PublicKey getPublicServerKey() const
    {
      return SodiumLib::AsymCrypto_PublicKey::asCopy(pk);
    }

    virtual unique_ptr<AbstractWorker> getNewWorker(int _fd, sockaddr_in _clientAddress) override
    {
      return make_unique<SrvWorker>(pk, sk, _fd, _clientAddress);
    }

  private:
    SodiumLib* sodium;
    SodiumLib::AsymCrypto_PublicKey pk;
    SodiumLib::AsymCrypto_SecretKey sk;
  };

  // prepare a simple client
  class SimpleClient : public Net::CryptoClientServer::CryptoClient
  {
  public:
    SimpleClient(SodiumLib::AsymCrypto_PublicKey& _pk, SodiumLib::AsymCrypto_SecretKey& _sk)
      :Net::CryptoClientServer::CryptoClient(_pk, _sk, "localhost", 11111) {}

    void pingpong(size_t nBytes)
    {
      ManagedBuffer out{nBytes};
      sodium->randombytes_buf(out);

      encryptAndWrite(out);

      SodiumSecureMemory returnCopy;
      PreemptiveReadResult rr;
      tie(rr, returnCopy) = readAndDecrypt(1000);

      ASSERT_TRUE(rr == PreemptiveReadResult::Complete);
      ASSERT_TRUE(returnCopy.isValid());
      ASSERT_TRUE(sodium->memcmp(out, returnCopy));
    }

  };

  //
  // the actual test case starts here
  //

  // create a factory
  SrvWorkerFactory f;

  // prep a server wrapper on localhost:11111
  // and run it in a dedicated thread
  TcpServerWrapper wrp{"localhost", 11111, 5};
  thread tWrapper{[&](){ wrp.mainLoop(f); }};

  // instanciate a client and send a few
  // chunks of data
  SodiumLib* sodium = SodiumLib::getInstance();
  SodiumLib::AsymCrypto_PublicKey pk;
  SodiumLib::AsymCrypto_SecretKey sk;
  sodium->genAsymCryptoKeyPair(pk, sk);
  SimpleClient c{pk, sk};
  cout << "Testcase main: server public key is " << Sloppy::Crypto::toBase64(f.getPublicServerKey()) << endl;
  c.setExpectedServerKey(f.getPublicServerKey());
  ASSERT_TRUE(c.doAuthProcess());
  for (int i=0; i < 10; ++i)
  {
    c.pingpong(10000);
    cout << "Client: finished pingpong-iteration #" << i << endl;
  }

  // force-quit the server and its worker
  wrp.requestStop();
  cout << "Asked the wrapper to stop" << endl;
  tWrapper.join();
  cout << "The wrapper stopped." << endl;
}

//----------------------------------------------------------------------------

