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

#include "ManagedSocket.h"

using namespace std;

namespace Sloppy
{
  namespace Net
  {
    class TcpServerWrapper
    {
    public:
      TcpServerWrapper(const string& bindName, int port, size_t maxConCount);
      void mainLoop();

    private:
      ManagedSocket srvSocket;
    };

    //----------------------------------------------------------------------------

    class TcpClient
    {
    public:
      TcpClient(const string& _srvName, int _port);
      void connectAndRun();

    private:
      ManagedSocket cliSocket;
      string srvName;
      int port;
    };

  }
}
#endif
