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
        size_t thisReadDuration;

        if (timeout_ms > 0)
        {
          auto _elapsedTime = chrono::high_resolution_clock::now() - startTime;
          int elapsedTime = chrono::duration_cast<chrono::milliseconds>(_elapsedTime).count();

          int remainingTime = timeout_ms - elapsedTime;
          if (remainingTime <= 0)
          {
            break;
          }

          thisReadDuration = (remainingTime <= ReadTimeSlice_ms) ? remainingTime : ReadTimeSlice_ms;
        } else {
          thisReadDuration = ReadTimeSlice_ms;
        }

        string chunk;

        try
        {
          chunk = socket.blockingRead(nBytes, nBytes, thisReadDuration);
          data += chunk;
        }
        catch (ReadTimeout& e)
        {
          continue;   // a timeout is not bad. maybe we don't receive anything in this cycle but maybe in the next
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

    pair<AbstractWorker::PreemptiveReadResult, string> AbstractWorker::preemptiveRead_framed(size_t timeout_ms)
    {
      // start a stop watch
      auto startTime = chrono::high_resolution_clock::now();

      // read 4 byte, because they encode the number of following bytes
      PreemptiveReadResult rr;
      string sLen;
      tie(rr, sLen) = preemptiveRead(4, timeout_ms);

      // in case of any errors, simply return the error
      if (rr != PreemptiveReadResult::Complete)
      {
        return make_pair(rr, "");
      }

      // convert the string into and uint32 and then into a size_t
      uint32_t netByteCount = *((uint32_t*)sLen.c_str());
      size_t byteCount = ntohl(netByteCount);


      // calculate the remaining time and try to read the rest
      size_t remainingTime = 0;   // default: no timeout, read infinitely
      if (timeout_ms > 0)
      {
        auto _elapsed = chrono::high_resolution_clock::now() - startTime;
        size_t elapsed = chrono::duration_cast<chrono::milliseconds>(_elapsed).count();
        if (elapsed >= timeout_ms)
        {
          return make_pair(PreemptiveReadResult::Timeout, "");
        }

        remainingTime = timeout_ms - elapsed;
      }

      return preemptiveRead(byteCount, remainingTime);
    }

    //----------------------------------------------------------------------------

    bool AbstractWorker::write_framed(const string& data)
    {

      if ((data.size() >> 32) >= 1)
      {
        throw std::invalid_argument("Cannot send more that 2^32 bytes of framed data!");
      }

      uint32_t byteCount = (data.size() & 0xffffffff);
      uint32_t netByteCount = htonl(byteCount);
      string sLen{(char *)&netByteCount, 4};

      bool isOk = write(sLen);
      if (!isOk) return false;
      return write(data);
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
        tie(clientFd, cliAddr) = srvSocket.acceptNext(AcceptCycleTime_ms);

        // if the previous call wasn't a timeout, instanciate a new worker
        // for this connection
        if (clientFd >= 0)
        {
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
        }

        // after every connection or timeout
        // we cleanup our worker list
        auto itWorkerObject = workerObjects.begin();
        while (itWorkerObject != workerObjects.end())
        {
          AbstractWorker::WorkerStatus st = (*itWorkerObject)->getWorkerStatus();

          if (st == AbstractWorker::WorkerStatus::Done)
          {
            // calc the iterator to the thread object
            size_t idx = itWorkerObject - workerObjects.begin();
            auto itThread = workerThreads.begin() + idx;

            // the following call should return immediately
            (*itThread).join();

            // delete the container entries which deletes
            // the objects as well
            workerThreads.erase(itThread);
            itWorkerObject = workerObjects.erase(itWorkerObject);
          } else {
            ++itWorkerObject;
          }
        }
      } while (!isStopRequested);

      // we were requested to finish, so we kindly ask all our
      // worker threads to finish as well
      auto itWorkerObject = workerObjects.begin();
      while (itWorkerObject != workerObjects.end())
      {
        (*itWorkerObject)->requestQuit();
        ++itWorkerObject;
      }


      // and now we cross our fingers, hope for the best
      // and join all started worker threads
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
