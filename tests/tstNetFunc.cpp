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

#include <iostream>
#include <future>

#include <gtest/gtest.h>

#include "../Sloppy/Net/Net.h"

using namespace Sloppy::Net;

TEST(NetFuncs, MsgBuilder)
{
  MessageBuilder b;

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

  auto result = b.getData();

  MessageDissector d{result};

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
  vector<MessageBuilder> v;
  for (int i=0; i <10 ; ++i)
  {
    MessageBuilder msg;
    msg.addString(to_string(i));
    msg.addInt(i);

    v.push_back(msg);
  }

  MessageBuilder frame;
  frame.addString("SomeData");
  frame.addMessageList(v);
  frame.addString("SomeOtherData");


  MessageDissector d{frame.getDataAsRef()};
  ASSERT_EQ("SomeData", d.getString());
  vector<MessageDissector> vDis = d.getMessageList();
  ASSERT_EQ(10, vDis.size());
  int i = 0;
  for (MessageDissector& innerDis : vDis)
  {
    ASSERT_EQ(to_string(i), innerDis.getString());
    ASSERT_EQ(i, innerDis.getInt());
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

  using MyMsg = TypedMessageBuilder<MsgTypes>;
  using MyDis = TypedMessageDissector<MsgTypes>;

  MyMsg msg(MsgTypes::T2);
  msg.addString("SomeData");

  MyDis d{msg.getDataAsRef()};
  ASSERT_EQ(MsgTypes::T2, d.getType());
  ASSERT_EQ("SomeData", d.getString());
}
