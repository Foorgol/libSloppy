/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2021  Volker Knollmann
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

#include <math.h>                                      // for ceil
#include <stdint.h>                                    // for uint8_t, uint64_t
#include <stdio.h>                                     // for sprintf
#include <chrono>                                      // for duration, syst...
#include <cstring>                                     // for size_t, memcpy
#include <iostream>                                    // for operator<<, endl
#include <new>                                         // for bad_alloc
#include <stdexcept>                                   // for invalid_argument
#include <type_traits>                                 // for __strip_refere...
#include <vector>                                      // for vector

#include "../Memory.h"  // for MemView, MemArray

#include "Crypto.h"

using namespace std;

namespace Sloppy
{
  namespace Crypto
  {
    mt19937_64 rng = mt19937_64{static_cast<uint64_t>(chrono::system_clock::now().time_since_epoch().count())};

    string getRandomAlphanumString(size_t len)
    {
      if (len < 1) return "";

      string result;
      result.resize(len);

      static const char alphanum[] =
          "0123456789"
          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyz";

      for (size_t i = 0; i < len; ++i) {
        result[i] = alphanum[rng() % (sizeof(alphanum) - 1)];
      }

      return result;
    }

    //----------------------------------------------------------------------------

    string hashPassword_DEPRECATED(const string& pw, const string& salt, int numCycles)
    {
      if (numCycles < 1) return "";
      if (pw.empty()) return "";
      if (salt.empty()) return "";

      string s = salt + pw;
      for (int i=0; i < numCycles; ++i) s = SHA256::hash(s);

      return s;
    }

    //----------------------------------------------------------------------------

    pair<string, string> hashPassword_DEPRECATED(const string& pw, int saltLen, int numCycles)
    {
      if (saltLen < 1) return make_pair("","");

      string salt = getRandomAlphanumString(saltLen);
      string hashedPw = hashPassword_DEPRECATED(pw, salt, numCycles);

      if (hashedPw.empty()) return make_pair("", "");

      return make_pair(salt, hashedPw);
    }

    //----------------------------------------------------------------------------

    bool checkPassword_DEPRECATED(const string& clearPw, const string& hashedPw, const string& salt, int numCycles)
    {
      if (clearPw.empty() || hashedPw.empty() || salt.empty() || (numCycles < 0)) return false;

      string s = salt + clearPw;
      for (int i=0; i < numCycles; ++i) s = SHA256::hash(s);

      return (s == hashedPw);
    }

    //----------------------------------------------------------------------------

    size_t calc_base64_encSize(size_t rawSize)
    {
      // in Base64, every 6 "raw bit" become one "out byte"
      // ==> 33% overhead
      //
      // and: we're padding to multiples of four bytes output size
      return ceil(rawSize / 3.0) * 4;
    }

    //----------------------------------------------------------------------------

    size_t calc_base64_rawSize(size_t encSize, size_t paddingChars)
    {
      if ((encSize % 4) != 0) return 0; // error, padding not correct
      if (paddingChars > 2) return 0;  // can be either 0, 1 or 2

      // four encoded bytes become three raw bytes
      size_t result = 3 * encSize / 4;

      result -= paddingChars;

      return result;
    }

    //----------------------------------------------------------------------------

    size_t calc_base64_rawSize(const MemView& encData)
    {
      const size_t srcLen = encData.byteSize();
      if (srcLen < 4) return 0; // error, need at least four bytes

      size_t paddingChars = 0;
      size_t lastIdx = srcLen - 1;
      if (encData[lastIdx - 1] == '=') paddingChars = 2;
      else if (encData[lastIdx] == '=') paddingChars = 1;

      return calc_base64_rawSize(srcLen, paddingChars);
    }

    //----------------------------------------------------------------------------

    // Taken from https://stackoverflow.com/a/34571089
    MemArray toBase64(const MemView& src)
    {
      const size_t srcLen = src.byteSize();
      if (src.empty())
      {
        throw std::invalid_argument("toBase64: received empty source data array!");
      }

      // allocate the target memory
      MemArray dst{calc_base64_encSize(srcLen)};

      int val = 0;
      int valb = -6;

      size_t curSrcIdx = 0;
      size_t curDstIdx = 0;
      while (curSrcIdx < srcLen)
      {
        uint8_t c = src[curSrcIdx];

        val = (val<<8) + c;
        valb += 8;

        while (valb>=0) {
          dst[curDstIdx] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val>>valb)&0x3F];
          ++curDstIdx;
          valb -= 6;
        }

        ++curSrcIdx;
      }

      if (valb>-6)
      {
        dst[curDstIdx] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val<<8)>>(valb+8))&0x3F];
        ++curDstIdx;
      }

      while (curDstIdx % 4)
      {
        dst[curDstIdx] = '=';
        ++curDstIdx;
      }

      return dst;
    }

    //----------------------------------------------------------------------------

    string toBase64(const string &rawData)
    {
      if (rawData.empty()) return string{};

      MemArray enc = toBase64(MemView{rawData});

      // copy the data over to a string
      return string{enc.to_charPtr(), enc.byteSize()};
    }

    //----------------------------------------------------------------------------

    // Taken from https://stackoverflow.com/a/34571089
    MemArray fromBase64(const MemView& src)
    {
      // calculate the length of the destination data block and
      // make sure that the padding is used correctly in the source data
      size_t dstLen = calc_base64_rawSize(src);
      if (dstLen == 0)
      {
        throw std::invalid_argument("fromBase64: source data array is empty or malformed!");
      }

      MemArray dst{dstLen};

      vector<int> T(256,-1);
      for (int i=0; i<64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

      int val = 0;
      int valb =- 8;

      size_t curSrcIdx = 0;
      size_t curDstIdx = 0;
      while (curSrcIdx < src.byteSize())
      {
        uint8_t c = src[curSrcIdx];

        if (c == '=') break;  // we've reaching a padding character

        if (T[c] == -1)
        {
          throw std::invalid_argument("fromBase64: the source data array contains invalid, non-Base64 data!");
        }

        val = (val<<6) + T[c];
        valb += 6;

        if (valb>=0)
        {
          dst[curDstIdx] = static_cast<uint8_t>((val>>valb)&0xFF);
          ++curDstIdx;
          valb -= 8;
        }

        ++curSrcIdx;
      }

      return dst;

    }

    //----------------------------------------------------------------------------

    string fromBase64(const string& b64Data)
    {
      try
      {
        MemArray raw = fromBase64(MemView{b64Data});
        return string{raw.to_charPtr(), raw.byteSize()};
      }
      catch (std::bad_alloc& e)
      {
        // dump an error message and re-throw the exception
        cerr << e.what() << endl;
        throw;
      }
      catch (...) {}

      return string{};
    }

    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    /*
     * Everything for SHA256 implementation below this line. See header file
     * for original license text!
     *
     */

    const unsigned int SHA256::sha256_k[64] = //UL = uint32
    {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
        0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
        0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
        0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
        0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

    //----------------------------------------------------------------------------

    void SHA256::transform(const unsigned char *message, unsigned int block_nb)
    {
      uint32 w[64];
      uint32 wv[8];
      uint32 t1, t2;
      const unsigned char *sub_block;
      int i;
      int j;
      for (i = 0; i < (int) block_nb; i++) {
        sub_block = message + (i << 6);
        for (j = 0; j < 16; j++) {
          SHA2_PACK32(&sub_block[j << 2], &w[j]);
        }
        for (j = 16; j < 64; j++) {
          w[j] =  SHA256_F4(w[j -  2]) + w[j -  7] + SHA256_F3(w[j - 15]) + w[j - 16];
        }
        for (j = 0; j < 8; j++) {
          wv[j] = m_h[j];
        }
        for (j = 0; j < 64; j++) {
          t1 = wv[7] + SHA256_F2(wv[4]) + SHA2_CH(wv[4], wv[5], wv[6])
              + sha256_k[j] + w[j];
          t2 = SHA256_F1(wv[0]) + SHA2_MAJ(wv[0], wv[1], wv[2]);
          wv[7] = wv[6];
          wv[6] = wv[5];
          wv[5] = wv[4];
          wv[4] = wv[3] + t1;
          wv[3] = wv[2];
          wv[2] = wv[1];
          wv[1] = wv[0];
          wv[0] = t1 + t2;
        }
        for (j = 0; j < 8; j++) {
          m_h[j] += wv[j];
        }
      }
    }

    //----------------------------------------------------------------------------

    void SHA256::init()
    {
      m_h[0] = 0x6a09e667;
      m_h[1] = 0xbb67ae85;
      m_h[2] = 0x3c6ef372;
      m_h[3] = 0xa54ff53a;
      m_h[4] = 0x510e527f;
      m_h[5] = 0x9b05688c;
      m_h[6] = 0x1f83d9ab;
      m_h[7] = 0x5be0cd19;
      m_len = 0;
      m_tot_len = 0;
    }

    //----------------------------------------------------------------------------

    void SHA256::update(const unsigned char *message, unsigned int len)
    {
      unsigned int block_nb;
      unsigned int new_len, rem_len, tmp_len;
      const unsigned char *shifted_message;
      tmp_len = SHA224_256_BLOCK_SIZE - m_len;
      rem_len = len < tmp_len ? len : tmp_len;
      memcpy(&m_block[m_len], message, rem_len);
      if (m_len + len < SHA224_256_BLOCK_SIZE) {
        m_len += len;
        return;
      }
      new_len = len - rem_len;
      block_nb = new_len / SHA224_256_BLOCK_SIZE;
      shifted_message = message + rem_len;
      transform(m_block, 1);
      transform(shifted_message, block_nb);
      rem_len = new_len % SHA224_256_BLOCK_SIZE;
      memcpy(m_block, &shifted_message[block_nb << 6], rem_len);
      m_len = rem_len;
      m_tot_len += (block_nb + 1) << 6;
    }

    //----------------------------------------------------------------------------

    void SHA256::final(unsigned char *digest)
    {
      unsigned int block_nb;
      unsigned int pm_len;
      unsigned int len_b;
      int i;
      block_nb = (1 + ((SHA224_256_BLOCK_SIZE - 9)
                       < (m_len % SHA224_256_BLOCK_SIZE)));
      len_b = (m_tot_len + m_len) << 3;
      pm_len = block_nb << 6;
      memset(m_block + m_len, 0, pm_len - m_len);
      m_block[m_len] = 0x80;
      SHA2_UNPACK32(len_b, m_block + pm_len - 4);
      transform(m_block, block_nb);
      for (i = 0 ; i < 8; i++) {
        SHA2_UNPACK32(m_h[i], &digest[i << 2]);
      }
    }

    //----------------------------------------------------------------------------

    void SHA256::nextChunk(const MemView& input)
    {
      update(input.to_ucPtr(), input.size());
    }

    //----------------------------------------------------------------------------

    void SHA256::nextChunk(const string& input)
    {
      update(reinterpret_cast<const unsigned char *>(input.c_str()), input.length());
    }

    //----------------------------------------------------------------------------

    string SHA256::done()
    {
      unsigned char digest[SHA256::DIGEST_SIZE];
      memset(digest,0,SHA256::DIGEST_SIZE);

      final(digest);

      char buf[2*SHA256::DIGEST_SIZE+1];
      buf[2*SHA256::DIGEST_SIZE] = 0;
      for (unsigned int i = 0; i < SHA256::DIGEST_SIZE; i++)
        sprintf(buf+i*2, "%02x", digest[i]);

      return string(buf);
    }

    //----------------------------------------------------------------------------

    string SHA256::hash(const string& input)
    {
      MemView mv{input};
      return hash(mv);
    }

    //----------------------------------------------------------------------------

    string SHA256::hash(const MemView& input)
    {
      SHA256 ctx{};
      ctx.update(input.to_ucPtr(), input.size());
      return ctx.done();
    }

  }
}
