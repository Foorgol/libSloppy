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
#include <utility>

#include <sodium.h>

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

    class SodiumKeyLocked : public SodiumBasicException
    {
    public:
      SodiumKeyLocked(const string& context = string{})
        :SodiumBasicException{"a secret key could not be accessed because it was locked / guarded", context} {}
    };

    class SodiumInvalidKeysize : public SodiumBasicException
    {
    public:
      SodiumInvalidKeysize(const string& context = string{})
        :SodiumBasicException{"a key does not have the required length", context} {}
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
      SodiumSecureMemory()
        :ManagedMemory{}, type{SodiumSecureMemType::Normal},
          lib{nullptr}, curProtection{SodiumSecureMemAccess::NoAccess} {}
      SodiumSecureMemory(size_t _len, SodiumSecureMemType t);
      SodiumSecureMemory(const string& src, SodiumSecureMemType t);

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
      static SodiumSecureMemory asCopy(const SodiumSecureMemory& src);
      SodiumSecureMemory copy() const;

    protected:
      SodiumSecureMemType type;
      SodiumLib* lib;
      SodiumSecureMemAccess curProtection;

      void releaseMemory() override;
    };

    //----------------------------------------------------------------------------

    enum class SodiumKeyType
    {
      Secret,
      Public
    };

    template<SodiumKeyType kt, size_t keySize>
    class SodiumKey : public SodiumSecureMemory
    {
    public:
      SodiumKey()
        : SodiumSecureMemory{keySize, (kt == SodiumKeyType::Secret) ? SodiumSecureMemType::Guarded : SodiumSecureMemType::Normal},
          keyType{kt}
      {}

      // move constructor and move assignment
      SodiumKey<kt, keySize>(SodiumKey<kt, keySize>&& other) { *this = std::move(other); }
      virtual SodiumKey<kt, keySize>& operator =(SodiumKey<kt, keySize>&& other)
      {
          SodiumSecureMemory::operator =(std::move(other));
          keyType = other.keyType;
          return *this;
      }
      virtual SodiumKey<kt, keySize>& operator =(SodiumSecureMemory&& other)
      {
        if (other.getSize() != keySize)
        {
          throw SodiumInvalidKeysize{"move assignment of SodiumKey"};
        }
        SodiumSecureMemory::operator =(std::move(other));
        keyType = kt;
        return *this;
      }

      static SodiumKey<kt, keySize> asCopy(const SodiumKey<kt, keySize>& src)
      {
        if (!(src.canRead()))
        {
          throw SodiumKeyLocked{"creating a key copy"};
        }

        SodiumKey<kt, keySize> cpy;
        SodiumSecureMemAccess srcProtection = src.getProtection();
        if (kt == SodiumKeyType::Secret)
        {
          cpy.setAccess(SodiumSecureMemAccess::RW);
        }

        memcpy(cpy.rawPtr, src.rawPtr, src.len);

        if (kt == SodiumKeyType::Secret)
        {
          cpy.setAccess(srcProtection);
        }

        return cpy;
      }

    protected:
      SodiumKeyType keyType;

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

      // symmetric encryption
      int (*crypto_secretbox_easy)(unsigned char *c, const unsigned char *m,
                                unsigned long long mlen, const unsigned char *n,
                                const unsigned char *k);
      int (*crypto_secretbox_open_easy)(unsigned char *m, const unsigned char *c,
                                     unsigned long long clen, const unsigned char *n,
                                     const unsigned char *k);
      int (*crypto_secretbox_detached)(unsigned char *c, unsigned char *mac, const unsigned char *m,
                                    unsigned long long mlen, const unsigned char *n, const unsigned char *k);
      int (*crypto_secretbox_open_detached)(unsigned char *m, const unsigned char *c,
                                         const unsigned char *mac, unsigned long long clen,
                                         const unsigned char *n, const unsigned char *k);
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

      // symmetric encryption based on ManagedMemory
      using SecretBoxKeyType = SodiumKey<SodiumKeyType::Secret, crypto_secretbox_KEYBYTES>;
      using SecretBoxNonceType = SodiumKey<SodiumKeyType::Public, crypto_secretbox_NONCEBYTES>;
      ManagedBuffer crypto_secretbox_easy(const ManagedMemory& msg, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key);
      SodiumSecureMemory crypto_secretbox_open_easy(const ManagedMemory& cipher, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key,
                                                    SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked);
      pair<ManagedBuffer, ManagedBuffer> crypto_secretbox_detached(const ManagedMemory& msg, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key);
      SodiumSecureMemory crypto_secretbox_open_detached(const ManagedMemory& cipher, const ManagedMemory& mac, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key,
                                                        SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked);

      // symmetric encryption based on strings
      string crypto_secretbox_easy(const string& msg, const string& nonce, const string& key);
      string crypto_secretbox_open_easy(const string& cipher, const string& nonce, const string& key);
      pair<string, string> crypto_secretbox_detached(const string& msg, const string& nonce, const string& key);
      string crypto_secretbox_open_detached(const string& cipher, const string& mac, const string& nonce, const string& key);

      // symmetric encryption, mixed version (messages as strings, keys and nonces as ManagedMemory)
      string crypto_secretbox_easy(const string& msg, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key);
      string crypto_secretbox_open_easy(const string& cipher, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key);
      pair<string, string> crypto_secretbox_detached(const string& msg, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key);
      string crypto_secretbox_open_detached(const string& cipher, const string& mac, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key);

    protected:
      SodiumLib(void* _libHandle);
      static unique_ptr<SodiumLib> inst;
      void *libHandle;
      SodiumPtr sodium;
    };

    // a wrapper class for easy symmetric encryption / decryption
    class SodiumSecretBox
    {
    public:
      using KeyType = SodiumKey<SodiumKeyType::Secret, crypto_secretbox_KEYBYTES>;
      using NonceType = SodiumKey<SodiumKeyType::Public, crypto_secretbox_NONCEBYTES>;

      SodiumSecretBox(const KeyType& _key, const NonceType& _nonce);

      // encryption
      ManagedBuffer encryptCombined(const ManagedMemory& msg);
      pair<ManagedBuffer, ManagedBuffer> encryptDetached(const ManagedMemory& msg);
      string encryptCombined(const string& msg);
      pair<string, string> encryptDetached(const string& msg);

      // decryption
      SodiumSecureMemory decryptCombined(const ManagedMemory& cipher, SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked);
      SodiumSecureMemory decryptDetached(const ManagedMemory& cipher, const ManagedMemory& mac, SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked);
      string decryptCombined(const string& cipher);
      string decryptDetached(const string& cipher, const string& mac);

    protected:
      void setKeyLockState(bool setGuard);

    private:
      KeyType key;
      NonceType nonce;
      SodiumLib* lib;
    };

  }
}
#endif
