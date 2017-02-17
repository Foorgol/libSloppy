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

    //----------------------------------------------------------------------------

    uint64_t hton_sizet(const uint64_t& in)
    {
      uint32_t high = (in >> 32);
      uint32_t low = (in & 0xffffffff);

      uint32_t netHigh = htonl(high);
      uint32_t netLow = htonl(low);

      // My personal definition: high-word first!
      uint64_t result = netHigh;
      result = result << 32;
      result += netLow;

      return result;
    }

    //----------------------------------------------------------------------------

    uint64_t ntoh_sizet(const uint64_t& in)
    {
      uint32_t netHigh = (in >> 32);  // my personal definition: high-word first!
      uint32_t netLow = (in & 0xffffffff);

      uint32_t h = ntohl(netHigh);
      uint32_t l = ntohl(netLow);

      uint64_t result = h;
      result = result << 32;
      result += l;

      return result;
    }

    //----------------------------------------------------------------------------

    void MessageBuilder::addString(const string& s)
    {
      addUI64(s.size());
      data += ByteString{(uint8_t*)s.c_str(), s.size()};
    }

    //----------------------------------------------------------------------------

    void MessageBuilder::addByte(uint8_t b)
    {
      data += b;
    }

    //----------------------------------------------------------------------------

    void MessageBuilder::addInt(int i)
    {
      static_assert(sizeof(int) == 4, "Need 32-bit int!!");

      addUI32((uint32_t) i);
    }

    //----------------------------------------------------------------------------

    void MessageBuilder::addUI16(uint16_t u)
    {
      uint16_t networkOrder = htons(u);
      data += ByteString{(uint8_t*)&networkOrder, 2};
    }

    //----------------------------------------------------------------------------

    void MessageBuilder::addUI32(uint32_t u)
    {
      uint32_t networkOrder = htonl(u);
      data += ByteString{(uint8_t*)&networkOrder, 4};
    }

    //----------------------------------------------------------------------------

    void MessageBuilder::addUI64(uint64_t u)
    {
      uint32_t high = (u >> 32);
      addUI32(high);
      uint32_t low = (u & 0xffffffff);
      addUI32(low);
    }

    //----------------------------------------------------------------------------

    string MessageDissector::getString()
    {
      // get the string length
      assertSufficientData(8);
      size_t len = getUI64();

      // get the string itself
      assertSufficientData(len);
      string result{((char *)data.c_str() + offset), len};
      offset += len;

      return result;
    }

    //----------------------------------------------------------------------------

    uint8_t MessageDissector::getByte()
    {
      assertSufficientData(1);
      return data[offset++];
    }

    //----------------------------------------------------------------------------

    int MessageDissector::getInt()
    {
      uint32_t u = getUI32();

      return (int)u;
    }

    //----------------------------------------------------------------------------

    uint16_t MessageDissector::getUI16()
    {
      assertSufficientData(2);

      uint16_t networkOrder = *((uint16_t *)(data.c_str() + offset));
      offset += 2;

      return ntohs(networkOrder);
    }

    //----------------------------------------------------------------------------

    uint32_t MessageDissector::getUI32()
    {
      assertSufficientData(4);

      uint32_t networkOrder = *((uint32_t *)(data.c_str() + offset));
      offset += 4;

      return ntohl(networkOrder);
    }

    //----------------------------------------------------------------------------

    uint64_t MessageDissector::getUI64()
    {
      assertSufficientData(8);

      uint32_t high = getUI32();
      uint32_t low = getUI32();

      uint64_t result = high;
      result = result << 32;
      result += low;

      return result;
    }

    //----------------------------------------------------------------------------

    bool MessageDissector::getBool()
    {
      return (getByte() != 0);
    }

    //----------------------------------------------------------------------------

    void MessageDissector::assertSufficientData(size_t n) const
    {
      if (offset >= data.size()) throw InvalidMessageAccess{};
      size_t remaining = data.size() - offset;
      if (remaining < n) throw InvalidMessageAccess{};
    }


  }
}
