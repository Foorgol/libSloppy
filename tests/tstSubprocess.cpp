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

#include <string>

#include <gtest/gtest.h>

#include "../Sloppy/SubProcess.h"

using namespace std;

TEST(SubProcess, HappyPath)
{
  auto cmdResult = Sloppy::execCmd({"/usr/bin/echo", "This is a test"});
  ASSERT_EQ(0, cmdResult.rc);
  ASSERT_TRUE(cmdResult);
  ASSERT_EQ(1, cmdResult.out.size());
  ASSERT_EQ("This is a test", cmdResult.out[0]);
  ASSERT_TRUE(cmdResult.err.empty());
}

//----------------------------------------------------------------------------

TEST(SubProcess, Remote)
{
  auto cmdResult = Sloppy::execRemoteCmd({"/usr/bin/hostname"}, "lessing-vpn");
  ASSERT_EQ(0, cmdResult.rc);
  ASSERT_TRUE(cmdResult);
  ASSERT_EQ(1, cmdResult.out.size());
  ASSERT_EQ("lessing", cmdResult.out[0]);
  ASSERT_TRUE(cmdResult.err.empty());
}

//----------------------------------------------------------------------------

