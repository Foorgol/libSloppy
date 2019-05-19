/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2019  Volker Knollmann
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

#include "ManagedSocket.h"

namespace Sloppy
{
  namespace Net
  {
    void ManagedSocket::bind(const string &bindName, int port)
    {
      sockaddr_in sa = fillSockAddr(bindName, port);
      int rc = ::bind(fd, (sockaddr *)&sa, sizeof(sa));
      if (rc != 0)
      {
        throw IOError();
      }
    }

    //----------------------------------------------------------------------------

    void ManagedSocket::listen(size_t maxConnectionCount)
    {
      if (maxConnectionCount < 1)
      {
        throw invalid_argument("Invalid connection count for listen()");
      }

      int rc = ::listen(fd, maxConnectionCount);
      if (rc != 0)
      {
        throw IOError();
      }
    }

    //----------------------------------------------------------------------------

    pair<int, sockaddr_in> ManagedSocket::acceptNext(size_t timeout_ms)
    {
      sockaddr_in cliAddr;
      socklen_t clilen = sizeof(cliAddr);

      // if we have a timeout, wait for available
      // connections using select()
      if (timeout_ms > 0)
      {
        bool hasData = waitForReadOnDescriptor(fd, timeout_ms);

        if (!hasData)
        {
          bzero(&cliAddr, clilen);
          return make_pair(-1, cliAddr);   // fd == -1 indicates timeout
        }
      }

      int newFd = ::accept(fd, (sockaddr *)&cliAddr, &clilen);

      return make_pair(newFd, cliAddr);
    }

    //----------------------------------------------------------------------------

    void ManagedSocket::connect(const string& srvName, int srvPort)
    {
      sockaddr_in srvAddr = fillSockAddr(srvName, srvPort);

      int rc = ::connect(fd, (sockaddr *)&srvAddr, sizeof(srvAddr));

      if (rc != 0)
      {
        throw IOError();
      }
    }

  }
}
