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

#ifndef WIN32
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
#endif

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

    void OutMessage::addString(const string& s)
    {
      addUI64(s.size());
      data += ByteString{(uint8_t*)s.c_str(), s.size()};
    }

    //----------------------------------------------------------------------------

    void OutMessage::addByte(uint8_t b)
    {
      data += b;
    }

    //----------------------------------------------------------------------------

    void OutMessage::addInt(int i)
    {
      static_assert(sizeof(int) == 4, "Need 32-bit int!!");

      addUI32((uint32_t) i);
    }

    //----------------------------------------------------------------------------

    void OutMessage::addUI16(uint16_t u)
    {
      uint16_t networkOrder = htons(u);
      data += ByteString{(uint8_t*)&networkOrder, 2};
    }

    //----------------------------------------------------------------------------

    void OutMessage::addUI32(uint32_t u)
    {
      uint32_t networkOrder = htonl(u);
      data += ByteString{(uint8_t*)&networkOrder, 4};
    }

    //----------------------------------------------------------------------------

    void OutMessage::addUI64(uint64_t u)
    {
      uint32_t high = (u >> 32);
      addUI32(high);
      uint32_t low = (u & 0xffffffff);
      addUI32(low);
    }

    //----------------------------------------------------------------------------

    void OutMessage::addMemView(const ArrayView<uint8_t>& mv)
    {
      // is sufficient space left in the data area?
      size_t bytesRemaining = data.capacity() - data.size();
      if (bytesRemaining < mv.size())
      {
        data.reserve(data.size() + mv.size() + sizeof(size_t));
      }

      // append a length tag and the data itself
      addUI64(mv.size());
      data.append(mv.to_uint8Ptr(), mv.byteSize());
    }

    //----------------------------------------------------------------------------

    MemView OutMessage::view()
    {
      return MemView(data.c_str(), data.size());
    }

    //----------------------------------------------------------------------------

    void OutMessage::addByteString(const ByteString& bs)
    {
      addUI64(bs.size());
      data += bs;
    }

    //----------------------------------------------------------------------------

    void OutMessage::addMessageList(const vector<OutMessage>& msgList)
    {
      addUI64(msgList.size());
      for (const OutMessage& msg : msgList)
      {
        addByteString(msg.getDataAsRef());
      }
    }

    //----------------------------------------------------------------------------

    void OutMessage::rawPoke(const MemView& mv, size_t dstOffset)
    {
      if ((dstOffset + mv.byteSize()) > data.size())
      {
        throw std::out_of_range{"OutMessage: rawPoke would exceed message limits"};
      }
      memcpy( ((char *)(data.c_str())) + dstOffset, mv.to_charPtr(), mv.byteSize());
    }

    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    InMessage::InMessage(const MemView& v)
      :fullView{v}, curView{v}, data{}
    {
    }

    //----------------------------------------------------------------------------

    InMessage InMessage::fromDataCopy(const MemView& v)
    {
      // start with an empty message
      InMessage result{MemView{}};

      // copy the data to the new message
      result.data = MemArray(v);  // creates a deep copy

      // adjust the views
      MemView owningData{result.data.to_uint8Ptr(), result.data.size()};
      result.fullView = owningData;
      result.curView = owningData;

      return result;
    }

    //----------------------------------------------------------------------------

    estring InMessage::getString()
    {
      // get the string length
      assertSufficientData(8);
      size_t len = getUI64();

      // get the string itself
      assertSufficientData(len);
      estring result{curView.to_charPtr(), len};
      curView.chopLeft(len);

      return result;
    }

    //----------------------------------------------------------------------------

    ByteString InMessage::getByteString()
    {
      // get the string length
      assertSufficientData(8);
      size_t len = getUI64();

      // get the string itself
      assertSufficientData(len);
      ByteString result{curView.to_uint8Ptr(), len};
      curView.chopLeft(len);

      return result;
    }

    //----------------------------------------------------------------------------

    uint8_t InMessage::getByte()
    {
      assertSufficientData(1);
      uint8_t b = curView[0];
      curView.chopLeft(1);
      return b;
    }

    //----------------------------------------------------------------------------

    int InMessage::getInt()
    {
      uint32_t u = getUI32();

      return static_cast<int>(u);
    }

    //----------------------------------------------------------------------------

    uint16_t InMessage::getUI16()
    {
      assertSufficientData(2);
      const uint16_t* uPtr = reinterpret_cast<const uint16_t*>(curView.to_voidPtr());

      uint16_t networkOrder = *uPtr;
      curView.chopLeft(2);

      return ntohs(networkOrder);
    }

    //----------------------------------------------------------------------------

    uint32_t InMessage::getUI32()
    {
      uint32_t result = peekUI32();
      curView.chopLeft(4);

      return result;
    }

    //----------------------------------------------------------------------------

    uint32_t InMessage::peekUI32()
    {
      assertSufficientData(4);

      const uint32_t* uPtr = reinterpret_cast<const uint32_t*>(curView.to_voidPtr());

      uint32_t networkOrder = *uPtr;

      return ntohl(networkOrder);
    }

    //----------------------------------------------------------------------------

    uint64_t InMessage::getUI64()
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

    uint64_t InMessage::peekUI64()
    {
      InMessage sub{curView};

      return sub.getUI64();
    }

    //----------------------------------------------------------------------------

    bool InMessage::getBool()
    {
      return (getByte() != 0);
    }

    //----------------------------------------------------------------------------

    vector<InMessage> InMessage::getMessageList()
    {
      size_t cnt = getUI64();

      vector<InMessage> result;
      for (size_t i = 0; i < cnt; ++i)
      {
        ByteString bs = getByteString();
        result.push_back(InMessage::fromDataCopy(bs));
      }

      return result;
    }

    //----------------------------------------------------------------------------

    MemArray InMessage::getMemArray()
    {
      // get the buffer size
      assertSufficientData(8);
      size_t len = getUI64();

      // get the buffer itself
      assertSufficientData(len);
      auto buf = curView.slice_byCount(0, len);
      MemArray result{buf};
      curView.chopLeft(len);

      return result;
    }

    //----------------------------------------------------------------------------

    MemView InMessage::getMemView()
    {
      // get the buffer size
      assertSufficientData(8);
      size_t len = getUI64();

      // get the buffer itself
      assertSufficientData(len);
      auto buf = curView.slice_byCount(0, len);
      MemView result{buf.to_charPtr(), buf.size()};
      curView.chopLeft(len);

      return result;
    }

    //----------------------------------------------------------------------------

    void InMessage::assertSufficientData(size_t n) const
    {
      if (curView.size() < n)
      {
        throw std::out_of_range("Sloppy::Message: insufficient remaining data for read operation");
      }
    }

  }
}
