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

#include "../Sloppy/libSloppy.h"
#include "../Sloppy/Crypto/Crypto.h"
#include "../Sloppy/Memory.h"

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

//----------------------------------------------------------------------------

TEST(Crypto, Base64Enc)
{
  string src__0Padding{"Winter is coming!!"};
  string b64_expected__0Padding{"V2ludGVyIGlzIGNvbWluZyEh"};

  string src__1Padding{"42"};
  string b64_expected__1Padding{"NDI="};

  string src__2Padding{"The quick brown fox jumps over the lazy dog"};
  string b64_expected__2Padding{"VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZw=="};

  vector<pair<string, string>> tstStrings = {
    {src__0Padding, b64_expected__0Padding},
    {src__1Padding, b64_expected__1Padding},
    {src__2Padding, b64_expected__2Padding}
  };

  for (pair<string, string>& stringSet : tstStrings)
  {
    string src = stringSet.first;
    string b64_expected = stringSet.second;

    //
    // encode to base64
    //

    // string --> base64-string
    string b64 = toBase64(src);
    ASSERT_EQ(b64_expected, b64);

    /*
    // buffer --> base64-string
    Sloppy::ManagedBuffer buf{src};
    b64 = toBase64(buf);
    ASSERT_EQ(b64_expected, b64);

    // buffer --> base64-buffer
    Sloppy::ManagedBuffer buf2{b64.size()};
    ASSERT_TRUE(toBase64(buf, buf2));
    ASSERT_EQ(b64_expected, buf2.copyToString());

    // buffer --> base64-buffer, too small
    buf2 = Sloppy::ManagedBuffer{b64.size() - 1};
    ASSERT_FALSE(toBase64(buf, buf2)); */

    //
    // decode from base64
    //

    // base64-string --> string
    string s = fromBase64(b64_expected);
    ASSERT_EQ(src, s);

    /*
    // base64-buffer --> buffer
    buf = Sloppy::ManagedBuffer{b64_expected};
    buf2 = Sloppy::ManagedBuffer{src.size()};
    ASSERT_TRUE(fromBase64(buf, buf2));
    ASSERT_EQ(src, buf2.copyToString());*/


    // test length calculations
    ASSERT_EQ(b64_expected.size(), calc_base64_encSize(src.size()));
    ASSERT_EQ(src.size(), calc_base64_rawSize(Sloppy::MemView{b64_expected}));
  }

}

//----------------------------------------------------------------------------

TEST(Crypto, Sha256_Hashing)
{
  const string data{"This is some dummy data!"};
  const string hash{"5eb6cb6459194396e4a9056c191a4056a85d050a5368c30035a117f7f771a3da"};  // determined with sha256sum

  // test the static function
  ASSERT_EQ(hash, SHA256::hash(data));

  // test the incremental hashing
  SHA256 h;
  h.nextChunk("This is ");
  h.nextChunk("some dummy ");
  h.nextChunk("data!");
  ASSERT_EQ(hash, h.done());
}
