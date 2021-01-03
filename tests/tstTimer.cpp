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
#include <thread>

#include <gtest/gtest.h>

#include "../Sloppy/Timer.h"

using namespace std;

TEST(Timer, BasicUsage)
{
  Sloppy::Timer t;
  this_thread::sleep_for(chrono::milliseconds(700));
  t.stop();

  ASSERT_TRUE(t.getTime__ms() >= 700);
  ASSERT_TRUE(t.getTime__ms() < 710);
  ASSERT_TRUE(t.getTime__us() >= 700000);
  ASSERT_TRUE(t.getTime__us() < 710000);
  ASSERT_TRUE(t.getTime__ns() >= 700000000);
  ASSERT_TRUE(t.getTime__ns() < 710000000);
  ASSERT_TRUE(t.getTime__secs() == 0);
  ASSERT_TRUE(t.getTime__secsDouble() >= 0.7);
  ASSERT_TRUE(t.getTime__secsDouble() < 0.71);
  ASSERT_TRUE(t.getTime__msDouble() >= 700.0);
  ASSERT_TRUE(t.getTime__msDouble() < 710.0);

  // restart
  t.restart();
  t.stop();
  ASSERT_LT(t.getTime__us(), 10);
}

//----------------------------------------------------------------------------

TEST(Timer, Timeouts)
{
  Sloppy::Timer t;
  t.setTimeoutDuration__ms(200);
  this_thread::sleep_for(chrono::milliseconds(100));
  ASSERT_FALSE(t.isElapsed());
  this_thread::sleep_for(chrono::milliseconds(150));
  ASSERT_TRUE(t.isElapsed());
}

//----------------------------------------------------------------------------

TEST(Timer, RemainingTime)
{
  Sloppy::Timer t;
  ASSERT_EQ(-1, t.getRemainingTime__ms());

  t.setTimeoutDuration__ms(70);
  this_thread::sleep_for(chrono::milliseconds(50));
  int64_t r = t.getRemainingTime__ms();
  ASSERT_TRUE(r <= 20);
  ASSERT_TRUE(r > 18);
  this_thread::sleep_for(chrono::milliseconds(50));
  ASSERT_EQ(0, t.getRemainingTime__ms());
}

