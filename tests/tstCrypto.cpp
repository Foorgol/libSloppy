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

#include <gtest/gtest.h>

#include "../Sloppy/Crypto/Crypto.h"

using namespace Sloppy::Crypto;

TEST(Crypto, GenRandomString)
{
  string s1 = getRandomAlphanumString(10);
  string s2 = getRandomAlphanumString(10);

  ASSERT_EQ(10, s1.size());
  ASSERT_EQ(10, s2.size());
  ASSERT_FALSE(s1 == s2);

  cout << "Random string 1: " << s1 << endl;
  cout << "Random string 2: " << s2 << endl;
}
