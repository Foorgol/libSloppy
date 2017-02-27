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
  class SrvWorker : public CryptoServer
  {
  public:
    SrvWorker(SodiumLib::AsymCrypto_PublicKey& _pk, SodiumLib::AsymCrypto_SecretKey& _sk, int _fd)
      :CryptoServer(_pk, _sk, _fd) {}

    virtual pair<CryptoClientServer::ResponseReaction, ManagedBuffer> handleRequest(const ManagedBuffer& reqData) override
    {
      return make_pair(ResponseReaction::SendAndContinue, ManagedBuffer::asCopy(reqData));
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
      return make_unique<SrvWorker>(pk, sk, _fd);
    }

  private:
    SodiumLib* sodium;
    SodiumLib::AsymCrypto_PublicKey pk;
    SodiumLib::AsymCrypto_SecretKey sk;
  };

  // prepare a simple client
  class SimpleClient : public CryptoClient
  {
  public:
    SimpleClient(SodiumLib::AsymCrypto_PublicKey& _pk, SodiumLib::AsymCrypto_SecretKey& _sk)
      :CryptoClient(_pk, _sk, "localhost", 11112) {}

    void pingpong(size_t nDWords)
    {
      MessageBuilder msg;
      for (size_t i = 0; i < nDWords; ++i)
      {
        msg.addUI64(i);
      }

      ManagedBuffer mb{msg.getSize()};
      memcpy(mb.get_uc(), msg.ucPtr(), msg.getSize());
      ASSERT_TRUE(encryptAndWrite(mb));
      cout << "Pingpong: " << msg.getSize() << " unencrypted Bytes sent to server" << endl;

      ManagedBuffer returnCopy;
      PreemptiveReadResult rr;
      tie(rr, returnCopy) = readAndDecrypt(5000);
      MessageDissector d{returnCopy};
      ASSERT_EQ(msg.getSize(), returnCopy.getSize());
      ASSERT_EQ(msg.getSize(), d.getSize());
      for (size_t i = 0; i < nDWords; ++i)
      {
        ASSERT_EQ(i, d.getUI64());
      }
      cout << "Pingpong: successfully checked " << nDWords << " 8-byte numbers" << endl;
    }

  };

  //
  // the actual test case starts here
  //

  // create a factory
  SrvWorkerFactory f;

  // prep a server wrapper on localhost:11112
  // and run it in a dedicated thread
  TcpServerWrapper wrp{"localhost", 11112, 5};
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
    c.pingpong(200000);
    cout << "Client: finished pingpong-iteration #" << i << endl;
  }
  c.closeSocket();

  // force-quit the server and its worker
  wrp.requestStop();
  cout << "Asked the wrapper to stop" << endl;
  tWrapper.join();
  cout << "The wrapper stopped." << endl;
}

//----------------------------------------------------------------------------

