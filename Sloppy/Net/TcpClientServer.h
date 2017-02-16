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

#ifndef SLOPPY__TCP_CLIENT_SERVER_H
#define SLOPPY__TCP_CLIENT_SERVER_H

#include <iostream>
#include <atomic>

#include "ManagedSocket.h"

using namespace std;

namespace Sloppy
{
  namespace Net
  {
    class AbstractWorker
    {
    public:
      enum class PreemptionStatus
      {
        Continue,   // go on serving
        Quit,       // stop when its conveniert, e.g. after finishing the current request
        Terminate   // stop as sonn as possible
      };
      enum class WorkerStatus
      {
        Ready,     // waiting to be started
        Running,   // started
        Done       // finished
      };

      AbstractWorker(int _fd)
        :socket{_fd}, ps{PreemptionStatus::Continue}, ws{WorkerStatus::Ready} {}

      AbstractWorker(ManagedSocket& s)
        :socket{s.releaseDescriptor()}, ps{PreemptionStatus::Continue}, ws{WorkerStatus::Ready} {}

      virtual ~AbstractWorker() {}

      // this is the actual thread function
      virtual void run() final;

      bool requestQuit();
      void requestTermination();

      WorkerStatus getWorkerStatus() { return ws; }
      PreemptionStatus getPreemptionStatus() { return ps; }

    protected:
      enum class PreemptiveReadResult
      {
        Complete,  // all requested data has been read
        Timeout,   // a timeout occured and the returned data is incomplete
        Interrupted, // the parent requested to QUIT or TERMINATE
        Error      // an IOError occured
      };

      pair<PreemptiveReadResult, string> preemptiveRead(size_t nBytes, size_t timeout_ms);
      bool write(const string& data);

      // this one has to be overriden by the specific worker
      virtual void doTheWork() {}

    private:
      static constexpr int ReadTimeSlice_ms = 10;
      ManagedSocket socket;
      atomic<PreemptionStatus> ps;
      atomic<WorkerStatus> ws;
    };

    //----------------------------------------------------------------------------

    class AbstractWorkerFactory
    {
    public:
      AbstractWorkerFactory() {}
      virtual ~AbstractWorkerFactory() {}

      virtual unique_ptr<AbstractWorker> getNewWorker(int _fd, sockaddr_in _clientAddress) = 0;
    };

    //----------------------------------------------------------------------------

    class TcpServerWrapper
    {
    public:
      static constexpr size_t AcceptCycleTime_ms = 100;
      TcpServerWrapper(const string& bindName, int port, size_t maxConCount);
      void mainLoop(AbstractWorkerFactory& workerFac);
      void requestStop() { isStopRequested = true; }

    private:
      ManagedSocket srvSocket;
      atomic<bool> isStopRequested;
    };

    //----------------------------------------------------------------------------

    class BasicTcpClient : public AbstractWorker
    {
    public:
      BasicTcpClient(const string& _srvName, int _port);

      static int getConnectedRawSocket(const string& srvName, int port);

    private:
      string srvName;
      int port;
    };

  }
}
#endif
