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

#ifndef SLOPPY__NET_H
#define SLOPPY__NET_H

#include <string>
#include <netdb.h>

#include "../libSloppy.h"

using namespace std;

namespace Sloppy
{
  namespace Net
  {
    class InvalidHostname{};

    using ByteString = basic_string<uint8_t>;

    sockaddr_in fillSockAddr(const string& hostName, int port);

    //----------------------------------------------------------------------------

    uint64_t hton_sizet(const uint64_t& in);
    uint64_t ntoh_sizet(const uint64_t& in);

    //----------------------------------------------------------------------------

    class MessageBuilder
    {
    public:
      MessageBuilder(){}

      void addString(const string& s);
      void addByte(uint8_t b);
      void addInt(int i);
      void addUI16(uint16_t u);
      void addUI32(uint32_t u);
      void addUI64(uint64_t u);
      void addBool(bool b) { addByte(b ? 1 : 0); }
      void addManagedMemory(const ManagedMemory& mem);
      void addByteString(const ByteString& bs);
      void addMessageList(const vector<MessageBuilder>& msgList);

      const ByteString& getDataAsRef() const { return data; }
      ByteString getData() const { return data; }

      const uint8_t* ucPtr() const { return data.c_str(); }
      const char* charPtr() const { return (char *)data.c_str(); }
      ManagedBuffer get() const;
      size_t getSize() const { return data.size(); }

      void clear() { data.clear(); }

    private:
      ByteString data;
    };

    //----------------------------------------------------------------------------

    class InvalidMessageAccess{};

    class MessageDissector
    {
    public:
      MessageDissector(const string& s)
        :data{(uint8_t *)s.c_str(), s.size()}, offset{0} {}
      MessageDissector(const ByteString& bs)
        :data{bs}, offset{0} {}
      MessageDissector(const ManagedMemory& mm)
        :data{mm.get_uc(), mm.getSize()}, offset{0} {}

      string getString();
      uint8_t getByte();
      int getInt();
      uint16_t getUI16();
      uint32_t getUI32();
      uint64_t getUI64();
      bool getBool();
      ManagedBuffer getManagedBuffer();
      ByteString getByteString();
      vector<MessageDissector> getMessageList();

      size_t getSize() const { return data.size(); }

    private:
      ByteString data;
      size_t offset;
      void assertSufficientData(size_t n) const;
    };

    //----------------------------------------------------------------------------

    template<typename TypeEnum>
    class TypedMessageBuilder : public MessageBuilder
    {
    public:
      TypedMessageBuilder(const TypeEnum& _msgType)
        :MessageBuilder{}, msgType{_msgType}
      {
        addInt(static_cast<int>(msgType));
      }

    private:
      TypeEnum msgType;
    };


    //----------------------------------------------------------------------------

    template<typename TypeEnum>
    class TypedMessageDissector : public MessageDissector
    {
    public:
      TypedMessageDissector(const string& s)
        :MessageDissector{s}, msgType{static_cast<TypeEnum>(getInt())} {}
      TypedMessageDissector(const ByteString& bs)
        :MessageDissector{bs}, msgType{static_cast<TypeEnum>(getInt())} {}
      TypedMessageDissector(const ManagedMemory& mm)
        :MessageDissector{mm}, msgType{static_cast<TypeEnum>(getInt())} {}

      // fake-construct an empty dissector to indicate errors to the caller
      TypedMessageDissector(TypeEnum t)
        :MessageDissector{""}, msgType{t} {}

      TypeEnum getType() const { return msgType; }

    private:
      TypeEnum msgType;
    };
  }
}

#endif
