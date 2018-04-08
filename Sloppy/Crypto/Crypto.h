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

#ifndef SLOPPY__CRYPTO_H
#define SLOPPY__CRYPTO_H

#include <string>
#include <tuple>
#include <random>
#include <chrono>

#include "../Memory.h"

using namespace std;

namespace Sloppy
{
  namespace Crypto
  {
    // a random engine that can be used by various crypto functions
    extern mt19937_64 rng;

    string getRandomAlphanumString(int len);

    pair<string, string> hashPassword_DEPRECATED(const string& pw, int saltLen, int numCycles);

    string hashPassword_DEPRECATED(const string& pw, const string& salt, int numCycles);

    bool checkPassword_DEPRECATED(const string& clearPw, const string& hashedPw, const string& salt, int numCycles);

    /** \brief Encodes a given memory block to Base64.
     *
     * \throws std::invalid_argument is the source data is empty (zero size).
     *
     * \throws std::bad_alloc if the memory for the encoded data could not be allocated
     *
     * \returns a MemArray that contains the encoded data.
     */
    MemArray toBase64(const MemView& src);

    /** \brief Encodes a string to Base64.
     *
     * \note Internally, the encoded data is copied once between an intermediate
     * memory array and the target string. This extra copy operation might be an
     * unacceptable penalty if dealing with large amounts of data. In that case,
     * consider using toBase64 with MemView / MemArry instead.
     *
     * \throws std::bad_alloc if the memory allocation for the intermediate storage
     * of the encoded data failed.
     *
     * \returns a string with the Base64-encoded source data. If the source string
     * was empty, the target string is empty as well.
     */
    string toBase64(const string& rawData);

    MemArray fromBase64(const MemView& src);
    string fromBase64(const string& b64Data);

    size_t calc_base64_encSize(size_t rawSize);

    size_t calc_base64_rawSize(size_t encSize, size_t paddingChars);
    size_t calc_base64_rawSize(const MemView& encData);

    /*
     * !!! Everything we need for SHA256 starts here !!!
     *
     *
     * Based on http://www.zedwood.com/article/cpp-sha256-function
     * Slightly modified so that the SHA256 does the calculation in a static function
     *
     * Original license text:
     *
     *
     * Updated to C++, zedwood.com 2012
     * Based on Olivier Gay's version
     * See Modified BSD License below:
     *
     * FIPS 180-2 SHA-224/256/384/512 implementation
     * Issue date:  04/30/2005
     * http://www.ouah.org/ogay/sha2/
     *
     * Copyright (C) 2005, 2007 Olivier Gay <olivier.gay@a3.epfl.ch>
     * All rights reserved.
     *
     * Redistribution and use in source and binary forms, with or without
     * modification, are permitted provided that the following conditions
     * are met:
     * 1. Redistributions of source code must retain the above copyright
     *    notice, this list of conditions and the following disclaimer.
     * 2. Redistributions in binary form must reproduce the above copyright
     *    notice, this list of conditions and the following disclaimer in the
     *    documentation and/or other materials provided with the distribution.
     * 3. Neither the name of the project nor the names of its contributors
     *    may be used to endorse or promote products derived from this software
     *    without specific prior written permission.
     *
     * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
     * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
     * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
     * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
     * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
     * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
     * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
     * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
     * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
     * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
     * SUCH DAMAGE.
     */

    /** \brief A class that wraps a SHA256 hasher
     */
    class SHA256
    {
    private:
      typedef unsigned char uint8;
      typedef unsigned int uint32;
      typedef unsigned long long uint64;

      const static uint32 sha256_k[];
      static const unsigned int SHA224_256_BLOCK_SIZE = (512/8);

      unsigned int m_tot_len;
      unsigned int m_len;
      unsigned char m_block[2*SHA224_256_BLOCK_SIZE];
      uint32 m_h[8];

    public:
      /** \brief Default ctor; initializes the hashing algo and prepares it for hashing the first data chunks
       */
      SHA256() { init(); }

      static const unsigned int DIGEST_SIZE = ( 256 / 8);

      /** \brief Updates the SHA256 hashing with the next chunk of data
       */
      void nextChunk(
          const MemView& input   ///< the data for hashing
          );

      /** \brief Updates the SHA256 hashing with the next chunk of data
       */
      void nextChunk(
          const string& input   ///< the data for hashing
          );

      /** \brief Finalizes the hashing and returns the hash value
       *
       * \note `nextChunk()` may not be used anymore after `done()` has been called
       *
       * \returns a string with the SHA256 hash value in hex notation (64 chars)
       */
      string done();

      /** \brief Calculates the SHA256 hash for a given string
       *
       * This is a static function that does not require the management of a
       * SHA256 instance by the caller.
       *
       * \returns a string with the SHA256 hash value in hex notation (64 chars)
       */
      static string hash(
          const string& input   ///< the input data for hashing
          );

      /** \brief Calculates the SHA256 hash for a given memory section
       *
       * This is a static function that does not require the management of a
       * SHA256 instance by the caller.
       *
       * \returns a string with the SHA256 hash value in hex notation (64 chars)
       */
      static string hash(
          const MemView& input   ///< the input data for hashing
          );

    protected:
      /** \brief Initializes / resets the hashing algo
       *
       * Needs to be called before any data can be hashed.
       */
      void init();

      /** \brief Hashes a chunk of data
       *
       * Can be called repeatedly if a sequence of data chunks shall be processed.
       */
      void update(
          const unsigned char *message,   ///< a pointer to the data location
          unsigned int len                ///< the number of bytes in that location
          );

      /** \brief Returns the current hash value
       *
       * \note the target array has to provide at least 32 bytes of space!
       * The is no length checking!
       */
      void final(
          unsigned char *digest   ///< the target to write the raw, binary hash value to
          );

      void transform(const unsigned char *message, unsigned int block_nb);
    };

    #define SHA2_SHFR(x, n)    (x >> n)
    #define SHA2_ROTR(x, n)   ((x >> n) | (x << ((sizeof(x) << 3) - n)))
    #define SHA2_ROTL(x, n)   ((x << n) | (x >> ((sizeof(x) << 3) - n)))
    #define SHA2_CH(x, y, z)  ((x & y) ^ (~x & z))
    #define SHA2_MAJ(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
    #define SHA256_F1(x) (SHA2_ROTR(x,  2) ^ SHA2_ROTR(x, 13) ^ SHA2_ROTR(x, 22))
    #define SHA256_F2(x) (SHA2_ROTR(x,  6) ^ SHA2_ROTR(x, 11) ^ SHA2_ROTR(x, 25))
    #define SHA256_F3(x) (SHA2_ROTR(x,  7) ^ SHA2_ROTR(x, 18) ^ SHA2_SHFR(x,  3))
    #define SHA256_F4(x) (SHA2_ROTR(x, 17) ^ SHA2_ROTR(x, 19) ^ SHA2_SHFR(x, 10))
    #define SHA2_UNPACK32(x, str)                 \
    {                                             \
      *((str) + 3) = (uint8) ((x)      );       \
      *((str) + 2) = (uint8) ((x) >>  8);       \
      *((str) + 1) = (uint8) ((x) >> 16);       \
      *((str) + 0) = (uint8) ((x) >> 24);       \
      }
    #define SHA2_PACK32(str, x)                   \
    {                                             \
      *(x) =   ((uint32) *((str) + 3)      )    \
      | ((uint32) *((str) + 2) <<  8)    \
      | ((uint32) *((str) + 1) << 16)    \
      | ((uint32) *((str) + 0) << 24);   \
      };

    //
    // SHA256 end
    //
  }
}

#endif
