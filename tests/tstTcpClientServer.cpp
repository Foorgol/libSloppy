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

#include <iostream>
#include <thread>

#include <gtest/gtest.h>

#include "../Sloppy/Net/TcpClientServer.h"

using namespace Sloppy;
using namespace Sloppy::Net;

TEST(TcpClientServer, HelloWorld)
{
  // prepare a server worker class that waits for a
  // "Hello" from the client and responds with "World"
  class SrvWorker : public AbstractWorker
  {
  public:
    SrvWorker(int _fd, sockaddr_in sa)
      :AbstractWorker{_fd} {}

    virtual void doTheWork() override
    {
      while(true)
      {
        PreemptiveReadResult rr;
        string data;
        tie(rr, data) = preemptiveRead_framed(1000);

        // quit if we don't receive enough data or
        // if we're requested to stop
        if (rr == PreemptiveReadResult::Timeout)
        {
          cout << "\t\tServerWorker: finishing due to read timeout!" << endl;
          return;
        }
        if (rr == PreemptiveReadResult::Error)
        {
          cout << "\t\tServerWorker: finishing due to read error!" << endl;
          return;
        }
        if (rr == PreemptiveReadResult::Interrupted)
        {
          cout << "\t\tServerWorker: finishing due to stop request!" << endl;
          return;
        }
        //ASSERT_EQ(PreemptiveReadResult::Complete, rr);

        // quit if we don't receive "Hello"
        /*if (data != "Hello")
        {
          cerr << "ServerWorker: received wrong request: " << data << endl;
          return;
        }*/
        ASSERT_EQ("Hello", data);

        // send a response
        ASSERT_TRUE(write_framed("World"));
      }
    }
  };

  // prepare a simple worker factory
  class SrvWorkerFactory : public AbstractWorkerFactory
  {
  public:
    SrvWorkerFactory()
      :AbstractWorkerFactory() {}

    virtual unique_ptr<AbstractWorker> getNewWorker(int _fd, sockaddr_in _clientAddress) override
    {
      return make_unique<SrvWorker>(_fd, _clientAddress);
    }
  };

  // prepare a simple client
  class SimpleClient : public AbstractWorker
  {
  public:
    SimpleClient()
      :AbstractWorker{getRawConnectedClientSocket("localhost", 11111)} {}

    virtual void doTheWork() override
    {
      constexpr int nRounds = 10;
      for (int i=0; i < nRounds; ++i)
      {
        ASSERT_TRUE(write_framed("Hello"));

        PreemptiveReadResult rr;
        string response;
        tie(rr, response) = preemptiveRead_framed(100);

        // quit if we don't receive enough data or
        // if we're requested to stop
        ASSERT_EQ(PreemptiveReadResult::Complete, rr);

        // quit if we don't receive "World"
        ASSERT_EQ("World", response);

        //cout << "Client: round " << i+1 << " of " << nRounds << " completed!" << endl;

        this_thread::sleep_for(chrono::milliseconds{50});
      }

      cout << "Client: all done!" << endl;
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

  // instanciate a client and start it in a new
  // thread
  SimpleClient c;
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

