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

#ifndef SLOPPY__MANAGED_SOCKET_H
#define SLOPPY__MANAGED_SOCKET_H

#include "../libSloppy.h"
#include "Net.h"

using namespace std;

namespace Sloppy
{
  namespace Net
  {

    class ManagedSocket : public ManagedFileDescriptor
    {
    public:
      enum class SocketType
      {
        TCP,
        UDP
      };

      ManagedSocket(SocketType t)
        :ManagedFileDescriptor{socket(AF_INET, (t == SocketType::UDP) ? SOCK_DGRAM : SOCK_STREAM, 0)}{}

      bool bind(const string& bindName, int port);
      bool listen(size_t maxConnectionCount = 1);
      pair<int, sockaddr_in> acceptNext();
      bool connect(const string& srvName, int srvPort);
    };

  }
}

#endif
