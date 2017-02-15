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

#include <stdexcept>
#include <string.h>

#include "Net.h"

namespace Sloppy
{
  namespace Net
  {

    sockaddr_in fillSockAddr(const string& hostName, int port)
    {
      if ((port < 1) || (port > 65535))
      {
        throw std::invalid_argument("Invalid port number");
      }

      sockaddr_in result;
      bzero(&result, sizeof(result));
      result.sin_family = AF_INET;

      if (hostName.empty())
      {
        result.sin_addr.s_addr = INADDR_ANY;
      } else {
        hostent* host = gethostbyname(hostName.c_str());
        if (host == nullptr)
        {
          throw InvalidHostname{};
        }
        bcopy((char *)host->h_addr, (char *)&result.sin_addr.s_addr, host->h_length);
      }
      result.sin_port = htons(port);

      return result;
    }
  }
}
