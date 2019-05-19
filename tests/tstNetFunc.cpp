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

#include <iostream>
#include <future>

#include <gtest/gtest.h>

#include "../Sloppy/Net/Net.h"
#include "../Sloppy/Memory.h"

using namespace Sloppy::Net;
using namespace std;

TEST(NetFuncs, MsgBuilder)
{
  OutMessage b;

  // ugly strings
  b.addString("");
  b.addString("ööüßABCD");

  // bool
  b.addBool(false);
  b.addBool(true);

  // bytes
  b.addByte(0);
  b.addByte(42);
  b.addByte(255);

  // uint16
  b.addUI16(0);
  b.addUI16(666);
  b.addUI16(65535);

  // uint32
  b.addUI32(0);
  b.addUI32(666);
  b.addUI32(0xffffffff);
  b.addUI32(0x12345678);

  // uint64
  b.addUI64(0);
  b.addUI64(666);
  b.addUI64(0xffffffffffffffff);
  b.addUI64(0x0123456789abcdef);

  // ints
  b.addInt(0);
  b.addInt(INT32_MIN);
  b.addInt(INT32_MAX);

  // MemView (storing it twice for retrieval as MemView and MemArray)
  string someData1{"abcdefg12345678"};
  string someData2{"zyx66666"};
  Sloppy::MemView mv1{someData1};
  Sloppy::MemView mv2{someData2};
  b.addMemView(mv1);
  b.addMemView(mv2);

  auto result = b.view();

  InMessage d{result};

  // ugly string
  ASSERT_EQ("", d.getString());
  ASSERT_EQ("ööüßABCD", d.getString());

  // bool
  ASSERT_FALSE(d.getBool());
  ASSERT_TRUE(d.getBool());

  // bytes
  ASSERT_EQ(0, d.getByte());
  ASSERT_EQ(42, d.getByte());
  ASSERT_EQ(255, d.getByte());

  // uint16
  ASSERT_EQ(0, d.getUI16());
  ASSERT_EQ(666, d.getUI16());
  ASSERT_EQ(65535, d.getUI16());

  // uint32
  ASSERT_EQ(0, d.getUI32());
  ASSERT_EQ(666, d.getUI32());
  ASSERT_EQ(0xffffffff, d.getUI32());
  ASSERT_EQ(0x12345678, d.getUI32());

  // uint64
  ASSERT_EQ(0, d.getUI64());
  ASSERT_EQ(666, d.getUI64());
  ASSERT_EQ(0xffffffffffffffff, d.getUI64());
  ASSERT_EQ(0x0123456789abcdef, d.getUI64());

  // ints
  ASSERT_EQ(0, d.getInt());
  ASSERT_EQ(INT32_MIN, d.getInt());
  ASSERT_EQ(INT32_MAX, d.getInt());

  // Get as MemView
  auto mv = d.getMemView();
  ASSERT_FALSE(mv == mv1);  // we should have different memory locations
  ASSERT_EQ(someData1.size(), mv.size());
  ASSERT_EQ(mv1.size(), mv.size());
  for (int i=0; i < mv.size(); ++i)
  {
    ASSERT_EQ(mv[i], mv1[i]);   // content should be identical
  }
  string s{mv.to_charPtr(), mv.size()};
  ASSERT_EQ(s, someData1);

  // get as MemArray
  auto ma = d.getMemArray();
  ASSERT_EQ(someData2.size(), ma.size());
  ASSERT_EQ(mv2.size(), ma.size());
  for (int i=0; i < ma.size(); ++i)
  {
    ASSERT_EQ(ma[i], mv2[i]);   // content should be identical
  }
  s = string{ma.to_charPtr(), ma.size()};
  ASSERT_EQ(s, someData2);

}

//----------------------------------------------------------------------------

TEST(NetFuncs, ByteArrayMessage_ByteString)
{
  static constexpr int nBytes = 1000;
  Sloppy::MemArray ba(nBytes);
  for (int i = 0; i < nBytes; ++i)
  {
    ba[i] = rand() % 256;
  }

  OutMessage b;
  b.addMemView(ba.view());
  ASSERT_EQ(nBytes + sizeof(size_t), b.getSize());

  InMessage m{b.view()};
  ByteString bs = m.getByteString();
  for (int i = 0; i < nBytes; ++i) ASSERT_EQ(bs[i], ba[i]);

  b.addByteString(bs);
  size_t expectedSize = 2 * (sizeof(size_t) + nBytes);
  ASSERT_EQ(expectedSize, b.getSize());
  auto bv = b.view();
  ASSERT_EQ(expectedSize, bv.byteSize());
  InMessage m2{bv};
  bs = m2.getByteString();   // get the first string, created via addArrayView
  bs = m2.getByteString();   // get the second string, created via addByteString
  ASSERT_EQ(nBytes, bs.size());
  for (int i = 0; i < nBytes; ++i) ASSERT_EQ(bs[i], ba[i]);
}

//----------------------------------------------------------------------------

TEST(NetFuncs, RawMessagePoke)
{
  OutMessage om;
  for (int i = 0; i < 100; ++i) om.addByte(120 + i);

  int i = 0x04030201;
  Sloppy::MemView v{(uint8_t*)&i, sizeof(int)};
  om.rawPoke(v, 42);
  auto mv = om.view();
  for (int i = 0; i < 42; ++i) ASSERT_EQ(120 + i, mv[i]);
  for (int i = 42; i < 46; ++i) ASSERT_EQ(i - 41, mv[i]);
  for (int i = 46; i < 100; ++i) ASSERT_EQ(120 + i, mv[i]);
}

//----------------------------------------------------------------------------

TEST(NetFuncs, OwningInMessage)
{
  OutMessage om;
  om.addByte(42);
  om.addString("SomeString");

  // create an InMessage with a local data copy
  InMessage im = InMessage::fromDataCopy(om.getDataAsRef());

  // double-clear the source data location
  char c{'x'};
  Sloppy::MemView mv{&c, 1};
  for (int i=0; i < om.getSize(); ++i) om.rawPoke(mv, i);  // overwrite existing memory
  om.clear();  // release memory

  // despite overwriting / clearing, we should see the original content
  ASSERT_EQ(42, im.getByte());
  ASSERT_EQ("SomeString", im.getString());
}

//----------------------------------------------------------------------------

TEST(NetFuncs, PeekDataWithoutForwarding)
{
  OutMessage om;
  om.addUI32(424242);
  om.addUI64(2323232323);

  InMessage im{om.getDataAsRef()};
  ASSERT_EQ(424242, im.peekUI32());
  ASSERT_EQ(424242, im.peekUI32());
  ASSERT_EQ(424242, im.getUI32());

  ASSERT_EQ(2323232323, im.peekUI64());
  ASSERT_EQ(2323232323, im.peekUI64());
  ASSERT_EQ(2323232323, im.getUI64());
}

//----------------------------------------------------------------------------

TEST(NetFuncs, Uint64Conversion)
{
  for (uint64_t u : vector<uint64_t>{0, 666, 0xffffffffffffffff, 0x0123456789abcdef})
  {
    uint64_t networkOrder = hton_sizet(u);
    uint64_t hostOrder = ntoh_sizet(networkOrder);

    ASSERT_EQ(u, hostOrder);
  }
}

//----------------------------------------------------------------------------

TEST(NetFuncs, MessageLists)
{
  vector<OutMessage> v;
  for (int i=0; i <10 ; ++i)
  {
    OutMessage msg;
    msg.addString(to_string(i));
    msg.addInt(i);

    v.push_back(msg);
  }

  OutMessage frame;
  frame.addString("SomeData");
  frame.addMessageList(v);
  frame.addString("SomeOtherData");


  InMessage d{frame.view()};
  ASSERT_EQ("SomeData", d.getString());
  vector<InMessage> vInMsg = d.getMessageList();
  ASSERT_EQ(10, vInMsg.size());
  int i = 0;
  for (InMessage& innerMsg : vInMsg)
  {
    ASSERT_EQ(to_string(i), innerMsg.getString());
    ASSERT_EQ(i, innerMsg.getInt());
    ++i;
  }
  ASSERT_EQ("SomeOtherData", d.getString());
}

//----------------------------------------------------------------------------

TEST(NetFuncs, TypedMessages)
{
  enum class MsgTypes
  {
    T1,
    T2,
    T3
  };

  using MyMsg = TypedOutMessage<MsgTypes>;
  using MyDis = TypedInMessage<MsgTypes>;

  MyMsg msg(MsgTypes::T2);
  msg.addString("SomeData");

  MyDis d{msg.getDataAsRef()};
  ASSERT_EQ(MsgTypes::T2, d.getType());
  ASSERT_EQ("SomeData", d.getString());

  msg.rewriteType(MsgTypes::T3);
  d = MyDis{msg.getDataAsRef()};
  ASSERT_EQ(MsgTypes::T3, d.getType());
  ASSERT_EQ("SomeData", d.getString());

  // construct invalid messages
  OutMessage om;
  om.addByte(42);
  ASSERT_THROW(MyDis{om.getDataAsRef()}, std::out_of_range);   // input too short
}

