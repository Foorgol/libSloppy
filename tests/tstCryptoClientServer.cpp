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

using namespace Sloppy;
using namespace Sloppy::Net;

TEST(CryptoClientServerDemo, HelloWorld)
{
  // prepare a server worker class that waits for a
  // "Hello" from the client and responds with "World"
  class SrvWorker : public Net::CryptoClientServer::CryptoServer
  {
  public:
    SrvWorker(SodiumLib::AsymCrypto_PublicKey& _pk, SodiumLib::AsymCrypto_SecretKey& _sk, int _fd, sockaddr_in sa)
      :Net::CryptoClientServer::CryptoServer(_pk, _sk, _fd, sa) {}
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

  // instanciate a client and start it in a new
  // thread
  SodiumLib* sodium = SodiumLib::getInstance();
  SodiumLib::AsymCrypto_PublicKey pk;
  SodiumLib::AsymCrypto_SecretKey sk;
  sodium->genAsymCryptoKeyPair(pk, sk);
  SimpleClient c{pk, sk};
  cout << "Testcase main: server public key is " << Sloppy::Crypto::toBase64(f.getPublicServerKey()) << endl;
  c.setExpectedServerKey(f.getPublicServerKey());
  thread tClient{[&](){c.run();}};

  // wait
  this_thread::sleep_for(chrono::seconds{3});

  // force-quit the server and its worker
  wrp.requestStop();
  cout << "Asked the wrapper to stop" << endl;
  tWrapper.join();
  cout << "The wrapper stopped." << endl;

  // wait for the client to finish
  tClient.join();

}

//----------------------------------------------------------------------------

