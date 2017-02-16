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

#include <unistd.h>
#include <thread>

#include "../libSloppy.h"
#include "TcpClientServer.h"

namespace Sloppy
{
  namespace Net
  {
    void AbstractWorker::run()
    {
      ws = WorkerStatus::Running;
      doTheWork();
      ws = WorkerStatus::Done;
    }

    //----------------------------------------------------------------------------

    bool AbstractWorker::requestQuit()
    {
      if (ps != PreemptionStatus::Terminate) ps = PreemptionStatus::Quit;

      return (ps == PreemptionStatus::Quit);
    }

    //----------------------------------------------------------------------------

    void AbstractWorker::requestTermination()
    {
      ps = PreemptionStatus::Terminate;
    }

    //----------------------------------------------------------------------------

    pair<AbstractWorker::PreemptiveReadResult, string> AbstractWorker::preemptiveRead(size_t nBytes, size_t timeout_ms)
    {
      string data;

      // start a stop watch
      auto startTime = chrono::high_resolution_clock::now();

      while (true)
      {
        auto _elapsedTime = chrono::high_resolution_clock::now() - startTime;
        int elapsedTime = chrono::duration_cast<chrono::milliseconds>(_elapsedTime).count();

        int remainingTime = timeout_ms - elapsedTime;
        if (remainingTime <= 0)
        {
          break;
        }

        size_t thisReadDuration = (remainingTime <= ReadTimeSlice_ms) ? remainingTime : ReadTimeSlice_ms;

        string chunk;

        try
        {
          chunk = socket.blockingRead(nBytes, nBytes, thisReadDuration);
          data += chunk;
        }
        catch (ReadTimeout& e)
        {
          continue;   // a time out is not bad. maybe we don't receive anything in this cycle but maybe in the next
        }
        catch (...)
        {
          cerr << strerror(errno) << endl;
          return make_pair(PreemptiveReadResult::Error, "");
        }

        // are we requested to stop?
        if (ps != PreemptionStatus::Continue)
        {
          return make_pair(PreemptiveReadResult::Interrupted, data);
        }

        // is the data complete?
        if (data.size() == nBytes) break;
      }

      return make_pair((data.size() == nBytes) ? PreemptiveReadResult::Complete : PreemptiveReadResult::Timeout, data);
    }

    //----------------------------------------------------------------------------

    bool AbstractWorker::write(const string& data)
    {
      return socket.blockingWrite(data);
    }

    //----------------------------------------------------------------------------

    TcpServerWrapper::TcpServerWrapper(const string& bindName, int port, size_t maxConCount)
      :srvSocket{ManagedSocket::SocketType::TCP}, isStopRequested{false}
    {
      if (!(srvSocket.bind(bindName, port)))
      {
        throw IOError{};
      }

      if(!(srvSocket.listen(maxConCount)))
      {
        throw IOError{};
      }
    }

    //----------------------------------------------------------------------------

    void TcpServerWrapper::mainLoop(AbstractWorkerFactory& workerFac)
    {
      vector<thread> workerThreads;
      vector<unique_ptr<AbstractWorker>> workerObjects;

      do
      {
        int clientFd;
        sockaddr_in cliAddr;

        cout << "\t\t\twrapper: wait for connect!" << endl;
        tie(clientFd, cliAddr) = srvSocket.acceptNext();
        cout << "\t\t\twrapper: Connection on fd " << clientFd << "!" << endl;

        unique_ptr<AbstractWorker> newWorker = workerFac.getNewWorker(clientFd, cliAddr);
        if (newWorker == nullptr)
        {
          // the factory refused to handle this connection. So we close
          // it immediately
          ::close(clientFd);
        } else {
          // start a new thread for this worker and store the
          // thread handle
          auto wrkPtr = newWorker.get();
          thread tWorker{[&](){wrkPtr->run();}};
          workerThreads.push_back(std::move(tWorker));
          workerObjects.push_back(std::move(newWorker));
        }
      } while (!isStopRequested);
      cout << "\t\t\twrapper: left while-loop" << endl;

      // join all started threads
      for (thread& t : workerThreads) t.join();

      // the worker objects will be automatically destroyed / freed
      // when the workerObjects container is destroyed
    }

    //----------------------------------------------------------------------------

    BasicTcpClient::BasicTcpClient(const string& _srvName, int _port)
      :AbstractWorker{getConnectedRawSocket(_srvName, _port)}, srvName{_srvName}, port{_port} {}

    //----------------------------------------------------------------------------

    int BasicTcpClient::getConnectedRawSocket(const string& srvName, int port)
    {
      ManagedSocket s{ManagedSocket::SocketType::TCP};

      cout << "\tClient: waiting to connect" << endl;
      s.connect(srvName, port);
      cout << "\tClient: connected, waiting for data" << endl;

      int tmp = s.releaseDescriptor();
      cout << "\t Client socket is " << tmp << endl;

      return tmp;
    }


  }
}
