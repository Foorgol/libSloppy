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

#ifndef SLOPPY__SODIUM_H
#define SLOPPY__SODIUM_H

#include <memory>
#include <string>

#include "../libSloppy.h"

using namespace std;

namespace Sloppy
{
  namespace Crypto
  {
    // forward declaration
    class SodiumLib;

    // custom exceptions for error handling
    class SodiumBasicException
    {
    public:
      SodiumBasicException(const string& _msg = string{}, const string& context = string{})
        :msg{_msg}
      {
        if (!(context.empty())) msg += " ; context: " + context;
      }

      string what() { return msg; }
      void say() { cerr << "Sodium Wrapper Exception: " << msg << endl; }

    protected:
      string msg;
    };

    class SodiumOutOfMemoryException : public SodiumBasicException
    {
    public:
      SodiumOutOfMemoryException(const string& context = string{})
        :SodiumBasicException{"out of memory", context} {}
    };

    class SodiumNotAvailableException : public SodiumBasicException
    {
    public:
      SodiumNotAvailableException()
        :SodiumBasicException{"libsodium is not available"} {}
    };

    class SodiumMemoryManagementException : public SodiumBasicException
    {
    public:
      SodiumMemoryManagementException(const string& context = string{})
        :SodiumBasicException{"could not change the state of protected and/or locked memory", context} {}
    };

    //----------------------------------------------------------------------------

    enum class SodiumSecureMemType
    {
      Normal,
      Locked,
      Guarded
    };

    enum class SodiumSecureMemAccess
    {
      NoAccess,
      RO,
      RW
    };

    class SodiumSecureMemory : public ManagedMemory
    {
    public:
      SodiumSecureMemory(size_t _len, SodiumSecureMemType t);

      // disable copy functions
      SodiumSecureMemory(const SodiumSecureMemory&) = delete; // no copy constructor
      SodiumSecureMemory& operator=(const SodiumSecureMemory&) = delete; // no copy assignment

      // move semantics
      SodiumSecureMemory(SodiumSecureMemory&& other); // move constructor
      virtual SodiumSecureMemory& operator=(SodiumSecureMemory&& other); // move assignment

      virtual ~SodiumSecureMemory();

      SodiumSecureMemType getType() const { return type; }
      SodiumSecureMemAccess getProtection() const { return curProtection; }
      bool canRead() const { return ((curProtection == SodiumSecureMemAccess::RO) || (curProtection == SodiumSecureMemAccess::RW)); }
      bool canWrite() const { return (curProtection == SodiumSecureMemAccess::RW); }
      bool canAccess() const { return (curProtection != SodiumSecureMemAccess::NoAccess); }

      bool setAccess(SodiumSecureMemAccess a);

      // create a copy of an already allocated memory region
      static SodiumSecureMemory asCopy(SodiumSecureMemory& src);

    protected:
      SodiumSecureMemType type;
      SodiumLib* lib;
      SodiumSecureMemAccess curProtection;

      void releaseMemory() override;
    };

    //----------------------------------------------------------------------------

    // a table of function pointers to the
    // sodium lib
    struct SodiumPtr
    {
      // intialization
      int (*init)(void);

      // helpers
      int (*memcmp)(const void * const b1_, const void * const b2_, size_t len);
      char* (*bin2hex)(char * const hex, const size_t hex_maxlen, const unsigned char * const bin, const size_t bin_len);
      int (*isZero)(const unsigned char *n, const size_t nlen);

      // memory management
      void (*memzero)(void * const pnt, const size_t len);
      int (*mlock)(void * const addr, const size_t len);
      int (*munlock)(void * const addr, const size_t len);
      void *(*malloc)(size_t size);
      void *(*allocarray)(size_t count, size_t size);
      void (*free)(void*);
      int (*mprotect_noaccess)(void*);
      int (*mprotect_readonly)(void*);
      int (*mprotect_readwrite)(void*);

      // random data
      uint32_t (*randombytes_random)(void);
      uint32_t (*randombytes_uniform)(const uint32_t upper_bound);
      void (*randombytes_buf)(void * const buf, const size_t size);
    };

    //----------------------------------------------------------------------------

    // a C++ wrapper around the functions of libsodium
    //
    // it's implemented as a singleton that implicitly takes care
    // of proper initialization and de-initialization
    class SodiumLib
    {
    public:
      static SodiumLib* getInstance();
      ~SodiumLib();

      // helpers
      bool memcmp(const ManagedMemory& b1, const ManagedMemory& b2) const;
      string bin2hex(const string& binData) const;
      string bin2hex(const ManagedBuffer& binData) const;
      bool isZero(const ManagedMemory& buf) const;

      // memory management
      void memzero(void * const pnt, const size_t len);
      int mlock(void * const addr, const size_t len);
      int munlock(void * const addr, const size_t len);
      void *malloc(size_t size);
      void *allocarray(size_t count, size_t size);
      void free(void* ptr);
      int mprotect_noaccess(void* ptr);
      int mprotect_readonly(void* ptr);
      int mprotect_readwrite(void* ptr);

      // random data
      uint32_t randombytes_random() const;
      uint32_t randombytes_uniform(const uint32_t upper_bound) const;
      void randombytes_buf(const ManagedMemory& buf) const;

    protected:
      SodiumLib(void* _libHandle);
      static unique_ptr<SodiumLib> inst;
      void *libHandle;
      SodiumPtr sodium;
    };

  }
}
#endif
