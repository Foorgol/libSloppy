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

#include "../libSloppy.h"
#include "TcpClientServer.h"

namespace Sloppy
{
  namespace Net
  {
    TcpServerWrapper::TcpServerWrapper(const string& bindName, int port, size_t maxConCount)
      :srvSocket{ManagedSocket::SocketType::TCP}
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

    void TcpServerWrapper::mainLoop()
    {
      int clientFd;
      sockaddr_in cliAddr;
      cout << "\t\t\tSrv: wait for connect!" << endl;
      tie(clientFd, cliAddr) = srvSocket.acceptNext();
      cout << "\t\t\tSrv: Connection on fd " << clientFd << "!" << endl;

      ManagedFileDescriptor mfd{clientFd};
      mfd.blockingWrite("Srv --> client\n");

      // client FD will be closed when we're leaving this scope

      // server FD will be closed when the TcpServerWrapper is destroyed
    }

    //----------------------------------------------------------------------------

    TcpClient::TcpClient(const string& _srvName, int _port)
      :cliSocket{ManagedSocket::SocketType::TCP}, srvName{_srvName},
       port{_port} {}

    //----------------------------------------------------------------------------

    void TcpClient::connectAndRun()
    {
      cout << "\tClient: waiting to connect" << endl;
      cliSocket.connect(srvName, port);

      cout << "\tClient: connected, waiting for data" << endl;

      this_thread::sleep_for(chrono::milliseconds{1000});

      string d = cliSocket.blockingRead(10, 20, 1000);

      cout << "\tClient: received: " << d << endl;
    }

  }
}
