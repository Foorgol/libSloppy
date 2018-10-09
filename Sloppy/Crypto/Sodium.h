/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2018  Volker Knollmann
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
#include <iostream>

#include <sodium.h>

//#include "../libSloppy.h"
#include "../Memory.h"

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

    class SodiumMemoryGuardException : public SodiumBasicException
    {
    public:
      SodiumMemoryGuardException(const string& context = string{})
        :SodiumBasicException{"attempt to access guarded, inaccessible secure memory", context} {}
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

    class SodiumInvalidKey : public SodiumBasicException
    {
    public:
      SodiumInvalidKey(const string& context = string{})
        :SodiumBasicException{"the provided key was empty or invalid", context} {}
    };

    class SodiumInvalidNonce : public SodiumBasicException
    {
    public:
      SodiumInvalidNonce(const string& context = string{})
        :SodiumBasicException{"the provided nonce was empty or invalid", context} {}
    };

    class SodiumInvalidCipher : public SodiumBasicException
    {
    public:
      SodiumInvalidCipher(const string& context = string{})
        :SodiumBasicException{"the provided cipher was empty or of invalid size", context} {}
    };

    class SodiumInvalidMessage : public SodiumBasicException
    {
    public:
      SodiumInvalidMessage(const string& context = string{})
        :SodiumBasicException{"the provided plain text message was empty", context} {}
    };

    class SodiumInvalidMac : public SodiumBasicException
    {
    public:
      SodiumInvalidMac(const string& context = string{})
        :SodiumBasicException{"the provided authentication tag (MAC) was empty or of invalid size", context} {}
    };

    class SodiumInvalidBuffer : public SodiumBasicException
    {
    public:
      SodiumInvalidBuffer(const string& context = string{})
        :SodiumBasicException{"a buffer did not have the required size", context} {}
    };

    class SodiumConversionError : public SodiumBasicException
    {
    public:
      SodiumConversionError(const string& context = string{})
        :SodiumBasicException{"the conversion between data formats failed", context} {}
    };

    class SodiumAES256GCMUnavail : public SodiumBasicException
    {
    public:
      SodiumAES256GCMUnavail(const string& context = string{})
        :SodiumBasicException{"AES256-GCM is not supported on this machine", context} {}
    };

    //----------------------------------------------------------------------------

    /** An enum that defines different types of secure memory
     */
    enum class SodiumSecureMemType
    {
      Normal,   ///< normal, unprotected memory that is being managed through standard `malloc()` and `free()`
      Locked,   ///< special memory that is protected from being swapped to disk
      Guarded   ///< guarded memory that can be set to NoAccess, read-only or read-write
    };

    /** An enum that defines the current access protection class of a SodiumSecureMemory instance
     */
    enum class SodiumSecureMemAccess
    {
      NoAccess,   ///< memory may be neither read nor written; only possible for `SodiumSecureMemType::Guarded`
      RO,   ///< memory is read-only
      RW    ///< memory can read and written
    };

    /** \brief A class that manages access-protected memory that is suitable for storing secret data.
     */
    class SodiumSecureMemory
    {
    public:
      /** \brief Default ctor; creates a zero-sized, invalid data block
       */
      SodiumSecureMemory() = default;

      /** \brief Ctor that allocates a given amount of memory of a certain protection class.
       *
       * If the memory is of type `guarded`, it is initialized with read-write access after creation.
       *
       * \throws SodiumNotAvailableException if the sodium lib could not be loaded
       *
       * \throws SodiumOutOfMemoryException if the secure memory could not be allocated or locked
       */
      SodiumSecureMemory(
          size_t _len,   ///< the number of bytes to allocate
          SodiumSecureMemType t   ///< the protection class to use for that memory
          );

      /** \brief Ctor that allocates memory of a certain protection class or initializes it
       * with the contents of a string.
       *
       * If the memory is of type `guarded`, it is initialized with read-write access after creation.
       *
       * \throws std::invalid_argument if the source string was empty
       *
       * \throws SodiumNotAvailableException if the sodium lib could not be loaded
       *
       * \throws SodiumOutOfMemoryException if the secure memory could not be allocated or locked
       */
      SodiumSecureMemory(const string& src, SodiumSecureMemType t);

      /** \brief Ctor that allocates memory of a certain protection class or initializes it
       * with the contents of a given memory block.
       *
       * If the memory is of type `guarded`, it is initialized with read-write access after creation.
       *
       * \throws std::invalid_argument if the source string was empty
       *
       * \throws SodiumNotAvailableException if the sodium lib could not be loaded
       *
       * \throws SodiumOutOfMemoryException if the secure memory could not be allocated or locked
       */
      SodiumSecureMemory(const MemView& src, SodiumSecureMemType t);

      /** \brief Disabled copy assignment; if you need a copy, make it
       * explicit using the copy ctor.
       */
      SodiumSecureMemory& operator=(const SodiumSecureMemory&) = delete; // no copy assignment

      /** \brief Copy ctor that creates a deep copy of an existing secure memory region
       *
       * I want copies of secure memory to be explicit. This is why I disabled the
       * copy assignment operator.
       *
       * \note If the source is guarded memory it needs to be set to read-only or read-write
       * access before calling this copy function.
       *
       * \throws SodiumOutOfMemoryException if the secure memory could not be allocated or locked
       *
       * \throws SodiumMemoryGuardException if the source memory is guarded against access
       *
       * \throws SodiumMemoryManagementException if a copy of guarded memory has been successfully
       * created but could not be access-protected afterwards
       */
      SodiumSecureMemory(const SodiumSecureMemory& other);

      /** \brief Move ctor
       *
       * Memory that is currently being managed by this instance is free'd
       * before the other instance's data is taken over.
       *
       */
      SodiumSecureMemory(
          SodiumSecureMemory&& other   ///< other SodiumSecureMemory instance whose contents shall be taken over
          );

      /** \brief Move assignment operator
       *
       * Memory that is currently being managed by this instance is free'd
       * before the other instance's data is taken over.
       *
       */
      virtual SodiumSecureMemory& operator=(
          SodiumSecureMemory&& other   ///< other SodiumSecureMemory instance whose contents shall be taken over
          );

      /** \brief dtor, release all managed ressources.
       */
      virtual ~SodiumSecureMemory();

      /** \returns the type (normal, locked, guarded) of the managed memory
       */
      SodiumSecureMemType getType() const { return type; }

      /** \returns the currently set access protection (RW, RO, NoAccess) of the managed memory
       */
      SodiumSecureMemAccess getProtection() const { return curProtection; }

      /** \returns `true` if it is currently possible to read from the memory; `false` otherwise
       */
      bool canRead() const { return ((curProtection == SodiumSecureMemAccess::RO) || (curProtection == SodiumSecureMemAccess::RW)); }

      /** \returns `true` if it is currently possible to write to the memory; `false` otherwise
       */
      bool canWrite() const { return (curProtection == SodiumSecureMemAccess::RW); }

      /** \returns `false` if it is currently impossible to access the memory in any way and `true` otherwise; only useful for guarded memory
       */
      bool canAccess() const { return (curProtection != SodiumSecureMemAccess::NoAccess); }

      /** \brief Changes the access protection for the managed memory.
       *
       * Since access protection is only possible for guarded memory, this function
       * will always return `false` if called on on-guarded (read: `normal` or `locked`) memory.
       *
       * \returns `true` if the new access type was set successfully and `false` otherwise
       */
      bool setAccess(
          SodiumSecureMemAccess a   ///< the new access protection type to assign to the memory
          );

      /** \returns the number of bytes in the managed memory block
       */
      size_t size() const { return nBytes; }

      /** \brief Compares the content of two SodiumSecureMemory blocks
       *
       * \note If guarded memory is involved it needs to be set to read-only or read-write
       * access before calling this comparison function. Of course, this applies to this instance
       * as well as to the other memory block that is used for comparison.
       *
       * \returns `true` if the size and content of the two memory blocks are identical; `false` otherwise
       */
      bool operator == (
          const SodiumSecureMemory& other   ///< the other memory block for comparison
          ) const;

      /** \brief Frees the memory managed by this instance.
       *
       * \throws std::runtime_error if the memory could not be properly released.
       *
       * \note This method is also called from the dtor. An exception in the dtor will
       * terminate the program!
       */
      void releaseMemory();

      /** \returns a MemView instance that represents the memory managed by this instance.
       *
       * \note Accessing the memory through the MemView requires that guarded memory has been
       * enabled for reading.
       */
      MemView toMemView() const;

      /** \brief Convenience function that exports the raw pointer for read/write (non-const) access.
       *
       * \returns The internal raw pointer, non-const
       */
      unsigned char* to_ucPtr_rw() const
      {
        return reinterpret_cast<unsigned char *>(rawPtr);
      }

      /** \brief Convenience function that exports the raw pointer for read-only (const) access.
       *
       * \returns The internal raw pointer, const
       */
      const unsigned char* to_ucPtr_ro() const
      {
        return reinterpret_cast<const unsigned char *>(rawPtr);
      }

      /** \returns `true` if the instance does not contain memory, `false` otherwise
       */
      bool empty() const
      {
        return ((rawPtr == nullptr) || (nBytes == 0));
      }

      /** \returns a MemArray representation of this buffer; the MemArray is not owning
       * and will thus not release the memory after use.
       *
       * This is intended for passing SodiumSecureMemory instances in a common format
       * (MemArray) as a parameter to other functions.
       */
      MemArray toNotOwningArray() const
      {
        return MemArray{rawPtr, nBytes};
      }

    protected:
      SodiumLib* lib;

    private:
      void* rawPtr{nullptr};
      size_t nBytes{0};
      SodiumSecureMemType type{SodiumSecureMemType::Normal};
      SodiumSecureMemAccess curProtection{SodiumSecureMemAccess::NoAccess};

    };

    //----------------------------------------------------------------------------

    /** \brief An enum class for distinguishing between secret (private) and public cryptographic keys
     */
    enum class SodiumKeyType
    {
      Secret,   ///< the key is a secret key (either a private, asymetric key or a symetric key)
      Public    ///< the key is a public asymetric key
    };

    /** \brief An enum class defining how a `SodiumKey`s
     * memory should be initialized
     */
    enum class SodiumKeyInitStyle
    {
      Random, // fill with random data
      Zeros // fill with zeros
    };

    /** \brief A template class for cryptographic keys of different kinds and sizes
     *
     * Secret keys will be stored in guarded, secure memory. Public keys will be stored in normal memory.
     * All memory is heap allocated.
     *
     * \tparam kt sets the key type (secret or public)
     *
     * \tparam keySize defines the size of the key in bytes
     */
    template<SodiumKeyType kt, size_t keySize>
    class SodiumKey : public SodiumSecureMemory
    {
    public:

      /** \brief Default ctor; allocates memory for the key according to the defined key size.
       *
       * Internally calls the ctor of SodiumSecureMemory for the actual memory allocation. Thus,
       * all exceptions of the SodiumSecureMemory ctor can occur here as well.
       *
       * By default key memory is not initialized. If you need memory initialization,
       * use the ctor that takes a 'SodiumKeyInitStyle' argument.
       *
       * The memory for secret keys is **not** protected after initialization. It can be directly
       * accessed for reading and writing.
       */
      SodiumKey()
        : SodiumSecureMemory{keySize, (kt == SodiumKeyType::Secret) ? SodiumSecureMemType::Guarded : SodiumSecureMemType::Normal} {}

      /** \brief Allocates memory for the key according to the defined key size
       * and initializes the key's memory.
       *
       * The memory for secret keys is **not** protected after initialization. It can be directly
       * accessed for reading and writing.
       */
      SodiumKey(SodiumKeyInitStyle initStyle)
        : SodiumKey{}
      {
        if (initStyle == SodiumKeyInitStyle::Random)
        {
          lib->randombytes_buf(toNotOwningArray());
        } else {
          lib->memzero(toNotOwningArray());
        }
      }

      /** \brief Move ctor, forwards all activity to the move assignment operator.
       */
      SodiumKey(
            SodiumKey&& other   ///< the other key whose content shall be moved to this instance
            ) { *this = std::move(other); }

      /** \brief Move assignment operator, takes over the content of another key and invalidates the other key
       */
      virtual SodiumKey& operator =(
            SodiumKey&& other   ///< the other key whose content shall be moved to this instance
            )
      {
          // call the base class' move operator that does the
          // actual memory transfer
          SodiumSecureMemory::operator =(std::move(other));

          // take over the other key's key type
          keyType = other.keyType;

          // done
          return *this;
      }

      /** \brief Move assignment operator that takes content directly from SodiumSecureMemory
       *
       * \throws SodiumInvalidKeysize if the size of the SodiumSecureMemory does not match the key size
       */
      virtual SodiumKey& operator =(
            SodiumSecureMemory&& other   ///< the memory block whose content will be taken over by us
            )
      {
        if (other.size() != keySize)
        {
          throw SodiumInvalidKeysize{"move assignment of SodiumKey"};
        }
        SodiumSecureMemory::operator =(std::move(other));
        return *this;
      }
      
      /** \brief Disabled copy assignment. Make copying of keys
       * explicit using the copy constructor.
       */
       virtual SodiumKey& operator=(const SodiumKey& other) = delete;

      /** \brief Copy ctor, creates a deep copy of a key.
       *
       * Calls internally the ctor for SodiumKey and can thus throw all exceptions
       * that are also thrown by the ctor.
       *
       * \throws SodiumKeyLocked if a secret shall be copied that is currently not read-accessible
       */
       SodiumKey(const SodiumKey& src)
         :SodiumKey{}
      {
        if (!(src.canRead()))
        {
          throw SodiumKeyLocked{"creating a key copy"};
        }

        SodiumSecureMemAccess srcProtection = src.getProtection();
        if (kt == SodiumKeyType::Secret)
        {
          setAccess(SodiumSecureMemAccess::RW);
        }

        memcpy(toNotOwningArray().to_voidPtr(), src.toNotOwningArray().to_voidPtr(), src.size());

        if (kt == SodiumKeyType::Secret)
        {
          setAccess(srcProtection);
        }
      }

      /** \brief Copies content from a string into the key, overwriting all existing key data.
       *
       * \returns `false` if the string length does not match the key size or if the key is not write-accessible.
       */
      bool fillFromString(
            const string& data   ///< the string containing the data to copy
            )
      {
        if (data.size() != keySize) return false;
        if (!(canWrite())) return false;
        memcpy(toNotOwningArray().to_voidPtr(), data.c_str(), keySize);

        return true;
      }

      /** \brief Copies content from a MemView into the key, overwriting all existing key data.
        *
        * \returns `false` if the length of the memory block does not match the key size or if the key is not write-accessible.
       */
      bool fillFromMemView(
            const MemView& data   ///< a MemView instance pointing to the data to copy
            )
      {
        if (data.size() != keySize) return false;
        if (!(canWrite())) return false;
        memcpy(to_ucPtr_rw(), data.to_voidPtr(), keySize);
        return true;
      }

      /** \returns Returns the key as a BASE64 encoded string
       */
      string toBase64() const
      {
        if (empty()) return "";
        MemArray b64 = lib->bin2Base64(toMemView());
        return string{b64.to_charPtr(), b64.size()};
      }

      /** \brief Copies content from a Base64 encoded string into the key, overwriting all existing key data.
        *
        * \returns `false` if the string could not be decoded, if the length of the memory block does not
        * match the key size or if the key is not write-accessible.
       */
      bool fillFromBase64(
            const string& b64   ///< a Base64 encoded string with key content
            )
      {
        if (b64.empty()) return false;

        string raw;
        try
        {
          raw = lib->base642Bin(b64);
        } catch (...) {
          return false;
        }

        return fillFromString(raw);
      }

    protected:
      SodiumKeyType keyType{kt};
    };

    //----------------------------------------------------------------------------

    /** \brief A table of function pointers to the sodium lib
     */
    struct SodiumPtr
    {
      // intialization
      int (*init)(void);

      // helpers
      int (*memcmp)(const void * const b1_, const void * const b2_, size_t len);
      char* (*bin2hex)(char * const hex, const size_t hex_maxlen, const unsigned char * const bin, const size_t bin_len);
      int (*hex2bin)(unsigned char * const bin, const size_t bin_maxlen, const char * const hex, const size_t hex_len,
                     const char * const ignore, size_t * const bin_len, const char ** const hex_end);
      char* (*bin2base64)(char * const b64, const size_t b64_maxlen, const unsigned char * const bin,
                          const size_t bin_len, const int variant);
      int (*base642bin)(unsigned char * const bin, const size_t bin_maxlen, const char * const b64, const size_t b64_len,
                        const char * const ignore, size_t * const bin_len, const char ** const b64_end, const int variant);
      int (*isZero)(const unsigned char *n, const size_t nlen);
      void (*increment)(unsigned char *n, const size_t nlen);
      void (*add)(unsigned char *a, const unsigned char *b, const size_t len);

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

      // authentication
      int (*crypto_auth)(unsigned char *out, const unsigned char *in,
                      unsigned long long inlen, const unsigned char *k);
      int (*crypto_auth_verify)(const unsigned char *h, const unsigned char *in,
                             unsigned long long inlen, const unsigned char *k);

      // authenticated encryption with additional data, ChaCha20 with Poly1305
      int (*crypto_aead_xchacha20poly1305_ietf_encrypt)(unsigned char *c, unsigned long long *clen,
                                                  const unsigned char *m, unsigned long long mlen,
                                                  const unsigned char *ad, unsigned long long adlen,
                                                  const unsigned char *nsec, const unsigned char *npub,
                                                  const unsigned char *k);
      int (*crypto_aead_xchacha20poly1305_ietf_decrypt)(unsigned char *m, unsigned long long *mlen,
                                                  unsigned char *nsec,
                                                  const unsigned char *c, unsigned long long clen,
                                                  const unsigned char *ad, unsigned long long adlen,
                                                  const unsigned char *npub, const unsigned char *k);
      int (*crypto_aead_xchacha20poly1305_ietf_encrypt_detached)(unsigned char *c, unsigned char *mac,
                                                           unsigned long long *maclen_p,
                                                           const unsigned char *m, unsigned long long mlen,
                                                           const unsigned char *ad, unsigned long long adlen,
                                                           const unsigned char *nsec, const unsigned char *npub,
                                                           const unsigned char *k);
      int (*crypto_aead_xchacha20poly1305_ietf_decrypt_detached)(unsigned char *m, unsigned char *nsec,
                                                           const unsigned char *c, unsigned long long clen,
                                                           const unsigned char *mac,
                                                           const unsigned char *ad, unsigned long long adlen,
                                                           const unsigned char *npub, const unsigned char *k);

      // authenticated encryption with additional data, AES-256 in GCM mode
      int (*crypto_aead_aes256gcm_is_available)(void);
      int (*crypto_aead_aes256gcm_encrypt)(unsigned char *c, unsigned long long *clen,
                                           const unsigned char *m, unsigned long long mlen,
                                           const unsigned char *ad, unsigned long long adlen,
                                           const unsigned char *nsec, const unsigned char *npub,
                                           const unsigned char *k);
      int (*crypto_aead_aes256gcm_decrypt)(unsigned char *m, unsigned long long *mlen_p,
                                           unsigned char *nsec,
                                           const unsigned char *c, unsigned long long clen,
                                           const unsigned char *ad, unsigned long long adlen,
                                           const unsigned char *npub, const unsigned char *k);
      int (*crypto_aead_aes256gcm_encrypt_detached)(unsigned char *c,
                                                    unsigned char *mac, unsigned long long *maclen_p,
                                                    const unsigned char *m, unsigned long long mlen,
                                                    const unsigned char *ad, unsigned long long adlen,
                                                    const unsigned char *nsec, const unsigned char *npub,
                                                    const unsigned char *k);
      int (*crypto_aead_aes256gcm_decrypt_detached)(unsigned char *m,
                                                    unsigned char *nsec,
                                                    const unsigned char *c, unsigned long long clen,
                                                    const unsigned char *mac,
                                                    const unsigned char *ad, unsigned long long adlen,
                                                    const unsigned char *npub, const unsigned char *k);

      // public key cryptography, key generation
      int (*crypto_box_keypair)(unsigned char *pk, unsigned char *sk);
      int (*crypto_box_seed_keypair)(unsigned char *pk, unsigned char *sk, const unsigned char *seed);
      int (*crypto_scalarmult_base)(unsigned char *q, const unsigned char *n);

      // public key cryptography, encryption / decryption
      int (*crypto_box_easy)(unsigned char *c, const unsigned char *m,
                             unsigned long long mlen, const unsigned char *n,
                             const unsigned char *pk, const unsigned char *sk);
      int (*crypto_box_open_easy)(unsigned char *m, const unsigned char *c,
                                  unsigned long long clen, const unsigned char *n,
                                  const unsigned char *pk, const unsigned char *sk);
      int (*crypto_box_detached)(unsigned char *c, unsigned char *mac,
                                 const unsigned char *m, unsigned long long mlen,
                                 const unsigned char *n, const unsigned char *pk,
                                 const unsigned char *sk);
      int (*crypto_box_open_detached)(unsigned char *m, const unsigned char *c,
                                      const unsigned char *mac, unsigned long long clen,
                                      const unsigned char *n, const unsigned char *pk,
                                      const unsigned char *sk);

      // public key signature
      int (*crypto_sign_keypair)(unsigned char *pk, unsigned char *sk);
      int (*crypto_sign_seed_keypair)(unsigned char *pk, unsigned char *sk, const unsigned char *seed);
      int (*crypto_sign)(unsigned char *sm, unsigned long long *smlen,
                         const unsigned char *m, unsigned long long mlen, const unsigned char *sk);
      int (*crypto_sign_open)(unsigned char *m, unsigned long long *mlen,
                              const unsigned char *sm, unsigned long long smlen, const unsigned char *pk);
      int (*crypto_sign_detached)(unsigned char *sig, unsigned long long *siglen,
                                  const unsigned char *m, unsigned long long mlen, const unsigned char *sk);
      int (*crypto_sign_verify_detached)(const unsigned char *sig, const unsigned char *m,
                                         unsigned long long mlen, const unsigned char *pk);
      int (*crypto_sign_ed25519_sk_to_seed)(unsigned char *seed, const unsigned char *sk);
      int (*crypto_sign_ed25519_sk_to_pk)(unsigned char *pk, const unsigned char *sk);

      // hashing
      int (*crypto_generichash)(unsigned char *out, size_t outlen,
                             const unsigned char *in, unsigned long long inlen,
                             const unsigned char *key, size_t keylen);
      int (*crypto_generichash_init)(crypto_generichash_state *state,
                                  const unsigned char *key,
                                  const size_t keylen, const size_t outlen);
      int (*crypto_generichash_update)(crypto_generichash_state *state,
                                    const unsigned char *in,
                                    unsigned long long inlen);
      int (*crypto_generichash_final)(crypto_generichash_state *state,
                                   unsigned char *out, const size_t outlen);
      size_t (*crypto_generichash_statebytes)(void);
      int (*crypto_shorthash)(unsigned char *out, const unsigned char *in,
                           unsigned long long inlen, const unsigned char *k);

      // password hashing / key derivation
      int (*crypto_pwhash)(unsigned char * const out, unsigned long long outlen,
                        const char * const passwd, unsigned long long passwdlen,
                        const unsigned char * const salt, unsigned long long opslimit,
                        size_t memlimit, int alg);
      int (*crypto_pwhash_str)(char out[crypto_pwhash_STRBYTES], const char * const passwd,
                            unsigned long long passwdlen, unsigned long long opslimit,
                            size_t memlimit);
      int (*crypto_pwhash_str_verify)(const char str[crypto_pwhash_STRBYTES],
                                   const char * const passwd, unsigned long long passwdlen);
      int (*crypto_pwhash_scryptsalsa208sha256)(unsigned char * const out, unsigned long long outlen,
                                             const char * const passwd, unsigned long long passwdlen,
                                             const unsigned char * const salt, unsigned long long opslimit,
                                             size_t memlimit);
      int (*crypto_pwhash_scryptsalsa208sha256_str)(char out[crypto_pwhash_scryptsalsa208sha256_STRBYTES],
                                                 const char * const passwd, unsigned long long passwdlen,
                                                 unsigned long long opslimit, size_t memlimit);
      int (*crypto_pwhash_scryptsalsa208sha256_str_verify)(const char str[crypto_pwhash_scryptsalsa208sha256_STRBYTES],
                                                        const char * const passwd, unsigned long long passwdlen);

      // Diffie-Hellmann key exchange
      int (*crypto_scalarmult)(unsigned char *q, const unsigned char *n, const unsigned char *p);

      // new cryptp_kx_* function for client/server key exchance
      int (*crypto_kx_keypair)(unsigned char pk[crypto_kx_PUBLICKEYBYTES],
                               unsigned char sk[crypto_kx_SECRETKEYBYTES]);
      int (*crypto_kx_seed_keypair)(unsigned char pk[crypto_kx_PUBLICKEYBYTES],
                                    unsigned char sk[crypto_kx_SECRETKEYBYTES],
                                    const unsigned char seed[crypto_kx_SEEDBYTES]);
      int (*crypto_kx_client_session_keys)(unsigned char rx[crypto_kx_SESSIONKEYBYTES],
                                           unsigned char tx[crypto_kx_SESSIONKEYBYTES],
                                           const unsigned char client_pk[crypto_kx_PUBLICKEYBYTES],
                                           const unsigned char client_sk[crypto_kx_SECRETKEYBYTES],
                                           const unsigned char server_pk[crypto_kx_PUBLICKEYBYTES]);
      int (*crypto_kx_server_session_keys)(unsigned char rx[crypto_kx_SESSIONKEYBYTES],
                                           unsigned char tx[crypto_kx_SESSIONKEYBYTES],
                                           const unsigned char server_pk[crypto_kx_PUBLICKEYBYTES],
                                           const unsigned char server_sk[crypto_kx_SECRETKEYBYTES],
                                           const unsigned char client_pk[crypto_kx_PUBLICKEYBYTES]);
    };

    //----------------------------------------------------------------------------

    /** \brief An enum class that defines different kinds of Base64-encodings
     * that are supported by libsodium
     */
    enum class SodiumBase64Enconding
    {
      Original,
      Original_NoPadding,
      URLSafe,
      URLSafe_NoPadding
    };

    /** \returns the libsodium-internal integer representation for a given
     * SodiumBase64Enconding value
     */
    int SodiumBase64Enconding2int(const SodiumBase64Enconding& enc);

    /** \brief A C++ wrapper around the functions of libsodium
     *
     * It is implemented as a singleton that implicitly takes care
     * of proper initialization and de-initialization of libsodium
     */
    class SodiumLib
    {
    public:
      /** \returns a pointer to the SodiumLib singleton instance
       */
      static SodiumLib* getInstance();

      /** \brief dtor, unloads the libsodium
       */
      ~SodiumLib();

      /** \brief No copy ctor, this is a singleton
       */
      SodiumLib(const SodiumLib& other) = delete;

      /** \brief No copy assignment, this is a singleton
       */
      SodiumLib& operator=(const SodiumLib& other) = delete;

      /** \brief Simple move ctor; transfers internal pointers to the
       * new object and deletes the pointers of the old instance
       */
      SodiumLib(SodiumLib&& other)
      {
        libHandle = other.libHandle;
        other.libHandle = nullptr;
        sodium = other.sodium;
        other.sodium = SodiumPtr{};   // all struct pointers should be initialized to nullptr
      }

      /** \brief Simple move assignment; transfers internal pointers to the
       * us and deletes the pointers of the other instance
       */
      SodiumLib& operator=(SodiumLib&& other)
      {
        libHandle = other.libHandle;
        other.libHandle = nullptr;
        sodium = other.sodium;
        other.sodium = SodiumPtr{};   // all struct pointers should be initialized to nullptr

        return *this;
      }

      /** \brief Compares two memory segments; original documentation [here](https://download.libsodium.org/doc/helpers).
       *
       * \note This function behaves a bit different than the underlying libsodium function:
       * if the sizes of the two memory segments do not match, we don't even look at the content at
       * all. If, for instance, one segment holds 20 bytes and the other 15, we *do not* compare
       * the first 15 bytes. Instead, we simply return `false`.
       *
       * \returns `true` if the two memory segments match in length and content; `false` otherwise
       */
      bool memcmp(
          const MemView& m1,   ///< the first memory segment for comparison
          const MemView& m2    ///< the second memory segment for comparison
          ) const;


      /** \brief Converts binary data into a string with a hexadecimal representation;
       * original documentation [here](https://download.libsodium.org/doc/helpers).
       *
       * \returns a string with a hex representation of the provided data (empty if the input was empty)
       */
      string bin2hex(
          const string& binData   ///< a string with the binary data for conversion
          ) const;

      /** \brief Converts binary data into a string with a hexadecimal representation;
       * original documentation [here](https://download.libsodium.org/doc/helpers).
       *
       * \note The returned array includes a terminating zero character! Thus, the resulting
       * array length for `n` bytes of input data is `2n + 1`!
       *
       * \returns a string with a hex representation of the provided data (empty if the input was empty)
       */
      MemArray bin2hex(
          const MemView& binData   ///< a memory block with the binary data for conversion
          ) const;

      /** \brief Converts a memory block with hexadecimal data into binary data;
       * original documentation [here](https://download.libsodium.org/doc/helpers).
       *
       * \note Especially if the hex data contains ignore characters, a final copying between
       * an intermediate array and the final result array is likely. Thus, you cannot rely on a constant
       * runtime of this function.
       *
       * \throws SodiumConversionError if the input string could not fully parsed, e.g. due to invalid characters
       *
       * \returns a MemArray with the raw bytes represented by the hex input string
       */
      MemArray hex2bin(
          const MemView& hex,   ///< a memory segment containing the hex data a for conversion
          const string& ignore = string{}   ///< an optional string with characters that should be ignored in the input string (e.g., newlines)
          ) const;

      /** \brief Converts a string with hexadecimal data into binary data;
       * original documentation [here](https://download.libsodium.org/doc/helpers).
       *
       * \note Especially if the hex data contains ignore characters, a final copying between
       * an intermediate array and the final result array is likely. Thus, you cannot rely on a constant
       * runtime of this function.
       *
       * \throws SodiumConversionError if the input string could not fully parsed, e.g. due to invalid characters
       *
       * \returns a string with the raw bytes represented by the hex input string
       */
      string hex2bin(
          const string& hex,   ///< a string containing the hex data a for conversion
          const string& ignore = string{}   ///< an optional string with characters that should be ignored in the input string (e.g., newlines)
          ) const;

      /** \brief Converts a string with binary data to a Base64-encoded string;
       * original documentation [here](https://download.libsodium.org/doc/helpers).
       *
       * \throws SodiumConversionError if the conversion failed. This should only happen
       * if the pre-calculated output size does not match the actual output size which is
       * more or less impossible and would indicate an error in libsodium itself.
       *
       * \returns a string containing the source data in the selected Base64 encoding
       */
      string bin2Base64(
          const string& bin,   ///< the binary data for conversion
          SodiumBase64Enconding enc = SodiumBase64Enconding::Original   ///< the encoding variant that shall be used
          ) const;

      /** \brief Converts a MemView with binary data to a Base64-encoded MemArray;
       * original documentation [here](https://download.libsodium.org/doc/helpers).
       *
       * \throws SodiumConversionError if the conversion failed. This should only happen
       * if the pre-calculated output size does not match the actual output size which is
       * more or less impossible and would indicate an error in libsodium itself.
       *
       * \returns a memory block containing the source data in the selected Base64 encoding
       */
      MemArray bin2Base64(
          const MemView& bin,   ///< the binary data for conversion
          SodiumBase64Enconding enc = SodiumBase64Enconding::Original   ///< the encoding variant that shall be used
          ) const;

      /** \brief Converts a string with Base64 encoded data into a string with binary data;
       * original documentation [here](https://download.libsodium.org/doc/helpers).
       *
       * \note Especially if the Base64 data contains ignore characters, a final copying between
       * an intermediate string and the final result string is likely. Thus, you cannot rely on a constant
       * runtime of this function.
       *
       * \throws SodiumConversionError if the conversion failed, e.g. due to invalid characters
       *
       * \returns a string with the binary data
       */
      string base642Bin(
          const string& b64,   ///< a string containing the Base64-encoded data
          const string& ignore = string{},   ///< a string with optional characters that shall be ignored during conversion (e.g., newlines)
          SodiumBase64Enconding enc = SodiumBase64Enconding::Original   ///< the Base64 encoding variant
          ) const;

      /** \brief Converts a memory block with Base64 encoded data into a memory block with binary data;
       * original documentation [here](https://download.libsodium.org/doc/helpers).
       *
       * \note Especially if the Base64 data contains ignore characters, a final copying between
       * an intermediate array and the final result array is likely. Thus, you cannot rely on a constant
       * runtime of this function.
       *
       * \throws SodiumConversionError if the conversion failed, e.g. due to invalid characters
       *
       * \returns a string with the binary data
       */
      MemArray base642Bin(
          const MemView& b64,   ///< a memory block containing the Base64-encoded data
          const string& ignore = string{},   ///< a string with optional characters that shall be ignored during conversion (e.g., newlines)
          SodiumBase64Enconding enc = SodiumBase64Enconding::Original   ///< the Base64 encoding variant
          ) const;

      /** \returns `true` if a given memory segment contains only zeros;
       * original documentation [here](https://download.libsodium.org/doc/helpers).
       */
      bool isZero(
          const MemView& buf   ///< the memory segment for checking
          ) const;

      /** \brief Increments an arbitrarily long unsigned number (represented by a memory segment) by 1;
       * original documentation [here](https://download.libsodium.org/doc/helpers).
       *
       * It runs in constant-time for a given length, and considers the number to be encoded in little-endian format.
       *
       */
      void increment(
          const MemArray& buf   ///< memory segment containing the arbitrarily long, unsigned number
          );

      /** \brief Adds two arbitrarily long unsigned numbers of equal length;
       * original documentation [here](https://download.libsodium.org/doc/helpers).
       *
       * The number are expected to be in little-endian. The first parameter is
       * overwritten with the result.
       */
      void add(
          const MemArray& a,   ///< first operand and target for the resulting number
          const MemView& b    ///< second operand, remains untouched
          );

      /** \brief Overwrites a memory segment with zeros;
       * original documentation [here](https://download.libsodium.org/doc/helpers/memory_management.html).
       */
      void memzero(
          const MemArray& buf   ///< the buffer to overwrite
          );

      /** \brief Locks a memory section and prevents it from being swapped to disk;
       * original documentation [here](https://download.libsodium.org/doc/helpers/memory_management.html).
       *
       * \returns `true` on success, `false` otherwise
       */
      bool mlock(const MemArray& buf);

      /** \brief Unlocks a memory section that has previously been locked by `mlock`;
       * original documentation [here](https://download.libsodium.org/doc/helpers/memory_management.html).
       *
       * Includes overwriting the memory with zeros.
       *
       * \returns `true` on success, `false` otherwise
       */
      bool munlock(const MemArray& buf);

      /** \brief Allocates a block of guarded memory for storing sensitive data;
       * original documentation [here](https://download.libsodium.org/doc/helpers/memory_management.html).
       *
       * \returns a raw pointer to the newly allocated memory
       */
      void *malloc(
          size_t size   ///< number of bytes to allocate
          );

      /** \brief Allocates a block of guarded memory for storing sensitive data;
       * original documentation [here](https://download.libsodium.org/doc/helpers/memory_management.html).
       *
       * \returns a raw pointer to the newly allocated memory
       */
      void *allocarray(
          size_t count,   ///< number of elements to allocate memory for
          size_t size     ///< size in bytes of each element
          );

      /** \brief Releases page-guarded memory that has previously been allocated
       * with libsodium`s `malloc()` or `allocarray()`;
       * original documentation [here](https://download.libsodium.org/doc/helpers/memory_management.html).
       */
      void free(void* ptr);

      /** \brief Makes page-guarded memory allocated via libsodium's `malloc()`
       * or `allocarray()` inaccessible (no read or write possible);
       * original documentation [here](https://download.libsodium.org/doc/helpers/memory_management.html).
       *
       * \returns the same value as the libsodium-internal call; however, the documentation doesn't specify
       * what the return value means (presumeably: 0 on success)
       */
      int mprotect_noaccess(void* ptr);

      /** \brief Sets page-guarded memory allocated via libsodium's `malloc()`
       * or `allocarray()` to read-only;
       * original documentation [here](https://download.libsodium.org/doc/helpers/memory_management.html).
       *
       * \returns the same value as the libsodium-internal call; however, the documentation doesn't specify
       * what the return value means (presumeably: 0 on success)
       */
      int mprotect_readonly(void* ptr);

      /** \brief Makes page-guarded memory allocated via libsodium's `malloc()`
       * or `allocarray()` fully accessible for reading and writing;
       * original documentation [here](https://download.libsodium.org/doc/helpers/memory_management.html).
       *
       * \returns the same value as the libsodium-internal call; however, the documentation doesn't specify
       * what the return value means (presumeably: 0 on success)
       */
      int mprotect_readwrite(void* ptr);

      /** \brief Generates a random, unsigned 32-bit integer value;
       * original documentation [here](https://download.libsodium.org/doc/generating_random_data).
       *
       * \returns an unpredictable value between `0` and `0xffffffff` (included).
       */
      uint32_t randombytes_random() const;


      /** \brief Generates a random unsigned integer uniformly distributed in a range
       * between zero and an upper bound;
       * original documentation [here](https://download.libsodium.org/doc/generating_random_data).
       *
       * \returns an unpredictable value between `0` and `upper_bound`, with `upper_bound` not being included.
       */
      uint32_t randombytes_uniform(const uint32_t upper_bound) const;

      /** \brief Fills a memory buffer with random data;
       * original documentation [here](https://download.libsodium.org/doc/generating_random_data).
       */
      void randombytes_buf(const MemArray& buf) const;

      // symmetric encryption based on MemView
      using SecretBoxKey = SodiumKey<SodiumKeyType::Secret, crypto_secretbox_KEYBYTES>;
      using SecretBoxNonce = SodiumKey<SodiumKeyType::Public, crypto_secretbox_NONCEBYTES>;
      using SecretBoxMac = SodiumKey<SodiumKeyType::Public, crypto_secretbox_MACBYTES>;

      /** \brief Symmetric, authenticated encryption of a plain message with the auth tag being attached to the cipher;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/authenticated_encryption.html).
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \returns a heap-allocated buffer with the encrypted data and an attached authentication tag
       */
      MemArray secretbox_easy(
          const MemView& msg,   ///< the plain message for encryption
          const SecretBoxNonce& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKey& key        ///< the secret key; the caller has to ensure that the memory is enabled for reading
          );

      /** \brief Symmetric, authenticated decryption of a cipher with attached auth tag;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/authenticated_encryption.html).
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidCipher if the provided cipher was of insufficient length (MAC + at least one message byte)
       *
       * \returns a secure memory buffer with the plain data
       * or an empty buffer if the decryption failed, e.g., due to an invalid MAC
       */
      SodiumSecureMemory secretbox_open_easy__secure(
          const MemView& cipher,   ///< the buffer with the encrypted message and the attached MAC
          const SecretBoxNonce& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKey& key,        ///< the secret key; the caller has to ensure that the memory is enabled for reading
          SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked   ///< the proctection class for the returned plain data buffer
          );

      /** \brief Symmetric, authenticated decryption of a cipher with attached auth tag;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/authenticated_encryption.html).
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidCipher if the provided cipher was of insufficient length (MAC + at least one message byte)
       *
       * \returns a heap-allocated standard buffer with the plain data
       * or an empty buffer if the decryption failed, e.g., due to an invalid MAC
       */
      MemArray secretbox_open_easy(
          const MemView& cipher,   ///< the buffer with the encrypted message and the attached MAC
          const SecretBoxNonce& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKey& key        ///< the secret key; the caller has to ensure that the memory is enabled for reading
          );

      /** \brief Symmetric, authenticated encryption of a plain message with the a separate, detached auth tag;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/authenticated_encryption.html).
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \returns two heap-allocated buffers, the first one containing the encrypted data and the second one the authentication tag
       */
      pair<MemArray, SecretBoxMac> secretbox_detached(
          const MemView& msg,   ///< the plain message for encryption
          const SecretBoxNonce& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKey& key        ///< the secret key; the caller has to ensure that the memory is enabled for reading
          );

      /** \brief Symmetric, authenticated decryption of a cipher with separate, detached auth tag;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/authenticated_encryption.html).
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \throws SodiumInvalidMac if the provided MAC was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidCipher if the provided cipher was of insufficient length (MAC + at least one message byte)
       *
       * \returns a secure memory buffer with the plain data
       * or an empty buffer if the decryption failed, e.g., due to an invalid MAC
       */
      SodiumSecureMemory secretbox_open_detached(
          const MemView& cipher,   ///< a buffer with the pure, encrypted message without MAC attachment
          const SecretBoxMac& mac,   ///< the MAC for the message
          const SecretBoxNonce& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKey& key,        ///< the secret key; the caller has to ensure that the memory is enabled for reading
          SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked   ///< the proctection class for the returned plain data buffer
          );

      /** \brief Symmetric, authenticated encryption of a plain message with the auth tag being attached to the cipher;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/authenticated_encryption.html).
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \returns a string with the encrypted data and an attached authentication tag
       */
      string secretbox_easy(
          const string& msg,   ///< a string with the plain text message for encryption
          const SecretBoxNonce& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKey& key        ///< the secret key; the caller has to ensure that the memory is enabled for reading
          );

      /** \brief Symmetric, authenticated decryption of a cipher with attached auth tag;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/authenticated_encryption.html).
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidCipher if the provided cipher was of insufficient length (MAC + at least one message byte)
       *
       * \returns a string with the plain data
       * or an empty buffer if the decryption failed, e.g., due to an invalid MAC
       */
      string secretbox_open_easy(
          const string& cipher,   ///< a string with the encrypted message and the attached MAC
          const SecretBoxNonce& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKey& key        ///< the secret key; the caller has to ensure that the memory is enabled for reading
          );

      /** \brief Symmetric, authenticated encryption of a plain message with the a separate, detached auth tag;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/authenticated_encryption.html).
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \returns a string with the encrypted message and a heap-allocated buffer containing the authentication tag
       */
      pair<string, SecretBoxMac> secretbox_detached(
          const string& msg,   ///< the plain message for encryption
          const SecretBoxNonce& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKey& key        ///< the secret key; the caller has to ensure that the memory is enabled for reading
          );


      /** \brief Symmetric, authenticated decryption of a cipher with separate, detached auth tag;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/authenticated_encryption.html).
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \throws SodiumInvalidMac if the provided MAC was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidCipher if the provided cipher was of insufficient length (MAC + at least one message byte)
       *
       * \returns a string with the plain data
       * or an empty string if the decryption failed, e.g., due to an invalid MAC
       */
      string secretbox_open_detached(
          const string& cipher,   ///< a string with the pure, encrypted message without MAC attachment
          const SecretBoxMac& mac,   ///< the MAC for the message
          const SecretBoxNonce& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKey& key        ///< the secret key; the caller has to ensure that the memory is enabled for reading
          );

      // message authentication
      using AuthKeyType = SodiumKey<SodiumKeyType::Secret, crypto_auth_KEYBYTES>;
      using AuthTagType = SodiumKey<SodiumKeyType::Public, crypto_auth_BYTES>;

      /** \brief Computes an authentication tag for a plain text message using a symmetric, secret key;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/secret-key_authentication.html).
       *
       * The computed tag can be verified with `crypto_auth_verify()`
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \returns a heap-allocated buffer containing the (public) authentication tag
       */
      AuthTagType auth(
          const MemView& msg,   ///< the input message
          const AuthKeyType& key   ///< the (confidential) key for computing the authentication tag
          );

      /** \brief Verifies the authentication tag of a plain text message using a symmetric, secret key;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/secret-key_authentication.html).
       *
       * The tag can be computed with `crypto_auth()`
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidMac if the provided authentication tag was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \returns `true` if the combination of message and tag was valid (and thus the message authentic); `false` otherwise
       */
      bool auth_verify(
          const MemView& msg,   ///< the input message
          const AuthTagType& tag,   ///< the previously computed authentication tag
          const AuthKeyType& key   ///< the (confidential) key for verifying the authentication tag
          );

      /** \brief Computes an authentication tag for a plain text message using a symmetric, secret key;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/secret-key_authentication.html).
       *
       * The computed tag can be verified with `crypto_auth_verify()`
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \returns a heap-allocated buffer containing the (public) authentication tag
       */
      AuthTagType auth(
          const string& msg,   ///< the input message
          const AuthKeyType& key   ///< the (confidential) key for computing the authentication tag
          );

      /** \brief Verifies the authentication tag of a plain text message using a symmetric, secret key;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/secret-key_authentication.html).
       *
       * The tag can be computed with `crypto_auth()`
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidMac if the provided authentication tag was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \returns `true` if the combination of message and tag was valid (and thus the message authentic); `false` otherwise
       */
      bool auth_verify(
          const string& msg,   ///< the input message
          const AuthTagType& tag,   ///< the previously computed authentication tag
          const AuthKeyType& key   ///< the (confidential) key for verifying the authentication tag
          );

      // authenticated encryption with additional data, buffer based
      using AEAD_XChaCha20Poly1305_KeyType = SodiumKey<SodiumKeyType::Secret, crypto_aead_xchacha20poly1305_ietf_KEYBYTES>;
      using AEAD_XChaCha20Poly1305_NonceType = SodiumKey<SodiumKeyType::Public, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES>;
      using AEAD_XChaCha20Poly1305_TagType = SodiumKey<SodiumKeyType::Public, crypto_aead_xchacha20poly1305_ietf_ABYTES>;


      /** \brief Symmetric, authenticated encryption with (optional) additional data;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/aead.html)
       * and especially [here](https://download.libsodium.org/doc/secret-key_cryptography/xchacha20-poly1305_construction.html).
       *
       * This function call uses the XChaCha20-Poly1305-IETF algorithm.
       *
       * The additional data is optional and will **not** be encrypted but it will be
       * included in the computation of the authentication tag.
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       */
      MemArray aead_xchacha20poly1305_encrypt(
          const MemView& msg,   ///< the plain message for encryption
          const AEAD_XChaCha20Poly1305_NonceType& nonce,   ///< the nonce to be used for this specific message
          const AEAD_XChaCha20Poly1305_KeyType& key,   ///< the secret key; the caller has to ensure that the memory is enabled for reading
          const MemView& ad = MemView{}   ///< optional, additional data to be included in the MAC computation
          );

      /** \brief Symmetric, authenticated decryption with (optional) additional data;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/aead.html)
       * and especially [here](https://download.libsodium.org/doc/secret-key_cryptography/xchacha20-poly1305_construction.html).
       *
       * This function call uses the XChaCha20-Poly1305-IETF algorithm.
       *
       * The additional data is optional will only be
       * included in the computation of the authentication tag.
       *
       * \throws SodiumInvalidCipher if the provided message was empty or too short
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \returns a secure memory buffer with the plain data
       * or an empty buffer if the decryption failed, e.g., due to an invalid MAC
       */
      SodiumSecureMemory aead_xchacha20poly1305_decrypt(
          const MemView& cipher,   ///< buffer with encrypted data and attached MAC
          const AEAD_XChaCha20Poly1305_NonceType& nonce,   ///< the nonce to be used for this specific message
          const AEAD_XChaCha20Poly1305_KeyType& key,   ///< the secret key; the caller has to ensure that the memory is enabled for reading
          const MemView& ad = MemView{},   ///< optional, additional data to be included in the MAC computation
          SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked   ///< the protection class for the resulting plain text
          );

      /** \brief Symmetric, authenticated encryption with (optional) additional data;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/aead.html)
       * and especially [here](https://download.libsodium.org/doc/secret-key_cryptography/xchacha20-poly1305_construction.html).
       *
       * This function call uses the XChaCha20-Poly1305-IETF algorithm.
       *
       * The additional data is optional and will **not** be encrypted but it will be
       * included in the computation of the authentication tag.
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       */
      string aead_xchacha20poly1305_encrypt(
          const string& msg,   ///< the plain message for encryption
          const AEAD_XChaCha20Poly1305_NonceType& nonce,   ///< the nonce to be used for this specific message
          const AEAD_XChaCha20Poly1305_KeyType& key,   ///< the secret key; the caller has to ensure that the memory is enabled for reading
          const string& ad = string{}   ///< optional, additional data to be included in the MAC computation
          );

      /** \brief Symmetric, authenticated decryption with (optional) additional data;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/aead.html)
       * and especially [here](https://download.libsodium.org/doc/secret-key_cryptography/xchacha20-poly1305_construction.html).
       *
       * This function call uses the XChaCha20-Poly1305-IETF algorithm.
       *
       * The additional data is optional will only be
       * included in the computation of the authentication tag.
       *
       * \throws SodiumInvalidCipher if the provided message was empty or too short
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \returns a secure memory buffer with the plain data
       * or an empty buffer if the decryption failed, e.g., due to an invalid MAC
       */
      string aead_xchacha20poly1305_decrypt(
          const string& cipher,   ///< buffer with encrypted data and attached MAC
          const AEAD_XChaCha20Poly1305_NonceType& nonce,   ///< the nonce to be used for this specific message
          const AEAD_XChaCha20Poly1305_KeyType& key,   ///< the secret key; the caller has to ensure that the memory is enabled for reading
          const string& ad = string{}   ///< optional, additional data to be included in the MAC computation
          );

      // authenticated encryption with additional data, AES-256 GCM, buffer based
      using AEAD_AES256GCM_KeyType = SodiumKey<SodiumKeyType::Secret, crypto_aead_aes256gcm_KEYBYTES>;
      using AEAD_AES256GCM_NonceType = SodiumKey<SodiumKeyType::Public, crypto_aead_aes256gcm_NPUBBYTES>;
      using AEAD_AES256GCM_TagType = SodiumKey<SodiumKeyType::Public, crypto_aead_aes256gcm_ABYTES>;

      /** \returns `true` if AES256-GCM is available on this CPU, `false` otherwise.
       *
       * \warning If this function returns `false` none of the crypto_aead_aes256gcm_* functions
       * may be used.
       */
      bool is_AES256GCM_avail();

      /** \brief Symmetric, authenticated encryption with (optional) additional data;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/aead.html)
       * and especially [here](https://download.libsodium.org/doc/secret-key_cryptography/aes-256-gcm.html).
       *
       * This function call uses AES256 in Gaulois Counter Mode (GCM).
       *
       * The additional data is optional and will **not** be encrypted but it will be
       * included in the computation of the authentication tag.
       *
       * \throws SodiumAES256GCMUnavail if AES256-GCM is not available on this machine
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       */
      MemArray aead_aes256gcm_encrypt(
          const MemView& msg,   ///< the plain message for encryption
          const AEAD_AES256GCM_NonceType& nonce,   ///< the nonce to be used for this specific message
          const AEAD_AES256GCM_KeyType& key,   ///< the secret key; the caller has to ensure that the memory is enabled for reading
          const MemView& ad = MemView{}   ///< optional, additional data to be included in the MAC computation
          );

      /** \brief Symmetric, authenticated decryption with (optional) additional data;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/aead.html)
       * and especially [here](https://download.libsodium.org/doc/secret-key_cryptography/aes-256-gcm.html).
       *
       * This function call uses AES256 in Gaulois Counter Mode (GCM).
       *
       * The additional data is optional will only be
       * included in the computation of the authentication tag.
       *
       * \throws SodiumAES256GCMUnavail if AES256-GCM is not available on this machine
       *
       * \throws SodiumInvalidCipher if the provided message was empty or too short
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \returns a secure memory buffer with the plain data
       * or an empty buffer if the decryption failed, e.g., due to an invalid MAC
       */
      SodiumSecureMemory aead_aes256gcm_decrypt(
          const MemView& cipher,   ///< buffer with encrypted data and attached MAC
          const AEAD_AES256GCM_NonceType& nonce,   ///< the nonce to be used for this specific message
          const AEAD_AES256GCM_KeyType& key,   ///< the secret key; the caller has to ensure that the memory is enabled for reading
          const MemView& ad = MemView{},   ///< optional, additional data to be included in the MAC computation
          SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked   ///< the protection class for the resulting plain text
          );

      /** \brief Symmetric, authenticated encryption with (optional) additional data;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/aead.html)
       * and especially [here](https://download.libsodium.org/doc/secret-key_cryptography/aes-256-gcm.html).
       *
       * This function call uses AES256 in Gaulois Counter Mode (GCM).
       *
       * The additional data is optional and will **not** be encrypted but it will be
       * included in the computation of the authentication tag.
       *
       * \throws SodiumAES256GCMUnavail if AES256-GCM is not available on this machine
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       */
      string aead_aes256gcm_encrypt(
          const string& msg,   ///< the plain message for encryption
          const AEAD_AES256GCM_NonceType& nonce,   ///< the nonce to be used for this specific message
          const AEAD_AES256GCM_KeyType& key,   ///< the secret key; the caller has to ensure that the memory is enabled for reading
          const string& ad = string{}   ///< optional, additional data to be included in the MAC computation
          );

      /** \brief Symmetric, authenticated decryption with (optional) additional data;
       * original documentation [here](https://download.libsodium.org/doc/secret-key_cryptography/aead.html)
       * and especially [here](https://download.libsodium.org/doc/secret-key_cryptography/aes-256-gcm.html).
       *
       * This function call uses AES256 in Gaulois Counter Mode (GCM).
       *
       * The additional data is optional will only be
       * included in the computation of the authentication tag.
       *
       * \throws SodiumAES256GCMUnavail if AES256-GCM is not available on this machine
       *
       * \throws SodiumInvalidCipher if the provided message was empty or too short
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \returns a secure memory buffer with the plain data
       * or an empty buffer if the decryption failed, e.g., due to an invalid MAC
       */
      string aead_aes256gcm_decrypt(
          const string& cipher,   ///< buffer with encrypted data and attached MAC
          const AEAD_AES256GCM_NonceType& nonce,   ///< the nonce to be used for this specific message
          const AEAD_AES256GCM_KeyType& key,   ///< the secret key; the caller has to ensure that the memory is enabled for reading
          const string& ad = string{}   ///< optional, additional data to be included in the MAC computation
          );

      // public key cryptography: keys, nonces, auth tags
      using AsymCrypto_PublicKey = SodiumKey<SodiumKeyType::Public, crypto_box_PUBLICKEYBYTES>;
      using AsymCrypto_SecretKey = SodiumKey<SodiumKeyType::Secret, crypto_box_SECRETKEYBYTES>;
      using AsymCrypto_KeySeed = SodiumKey<SodiumKeyType::Secret, crypto_box_SEEDBYTES>;
      using AsymCrypto_Nonce = SodiumKey<SodiumKeyType::Public, crypto_box_NONCEBYTES>;
      using AsymCrypto_Tag = SodiumKey<SodiumKeyType::Public, crypto_box_MACBYTES>;

      /** \brief Generates a new, random public / private key pair for asymetric cryptography;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/authenticated_encryption.html).
       *
       * \returns `true` if the provided buffers could be filled with the key data; `false` otherwise.
       */
      bool genAsymCryptoKeyPair(
          AsymCrypto_PublicKey& pk_out,   ///< reference to an externally created buffer that will be filled with the public key data
          AsymCrypto_SecretKey& sk_out   ///< reference to an externally created buffer that will be filled with the secret key data
          );

      /** \brief Generates a new, seeded public / private key pair for asymetric cryptography;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/authenticated_encryption.html).
       *
       * \returns `true` if the provided buffers could be filled with the key data; `false` otherwise.
       */
      bool genAsymCryptoKeyPairSeeded(
          const AsymCrypto_KeySeed& seed,   ///< the seed used for the key generation
          AsymCrypto_PublicKey& pk_out,   ///< reference to an externally created buffer that will be filled with the public key data
          AsymCrypto_SecretKey& sk_out   ///< reference to an externally created buffer that will be filled with the secret key data
          );

      /** \brief Computes the public key for asymetric cryptography from the associated secret key;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/authenticated_encryption.html).
       *
       * \returns `true` if the provided buffer could be filled with the key data; `false` otherwise.
       */
      bool genPublicCryptoKeyFromSecretKey(
          const AsymCrypto_SecretKey& sk,   ///< the secret key from which to derive the public key
          AsymCrypto_PublicKey& pk_out   ///< reference to an externally created buffer that will be filled with the public key data
          );

      /** \brief Authenticated, public-key encryption in combined mode;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/authenticated_encryption.html).
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if one of the provided key was empty
       *
       * \returns a heap-allocated buffer with the encrypted data and an attached authentication tag
       */
      MemArray box_easy(
          const MemView& msg,   ///< the plain input message
          const AsymCrypto_Nonce& nonce,   ///< the nonce to be used for this specific message
          const AsymCrypto_PublicKey& recipientKey,   ///< the recipient's public key
          const AsymCrypto_SecretKey& senderKey   ///< the sender's private key (for the authentication signature)
          );

      /** \brief Authenticated, public-key decryption in combined mode;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/authenticated_encryption.html).
       *
       * \throws SodiumInvalidCipher if the provided message was empty or too short
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if one of the provided key was empty
       *
       * \returns a heap-allocated buffer with the decrypted data or an invalid, empty buffer if the decryption failed
       */
      SodiumSecureMemory box_open_easy(
          const MemView& cipher,   ///< a memory buffer containing the cipher and the attached authentication tag
          const AsymCrypto_Nonce& nonce,   ///< the nonce to be used for this specific message
          const AsymCrypto_PublicKey& senderKey,   ///< the sender's public key for verifying the message integrity
          const AsymCrypto_SecretKey& recipientKey,   ///< the recipient's secret key for decrypting the message
          SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked   ///< the protection class for the plain message
          );

      /** \brief Authenticated, public-key encryption in detached mode;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/authenticated_encryption.html).
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if one of the provided key was empty
       *
       * \returns a pair of two heap-allocated buffers with the encrypted data and the authentication tag
       */
      pair<MemArray, AsymCrypto_Tag> box_detached(
          const MemView& msg,   ///< the plain input message
          const AsymCrypto_Nonce& nonce,   ///< the nonce to be used for this specific message
          const AsymCrypto_PublicKey& recipientKey,   ///< the recipient's public key
          const AsymCrypto_SecretKey& senderKey   ///< the sender's private key (for the authentication signature)
          );

      /** \brief Authenticated, public-key decryption in detached mode;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/authenticated_encryption.html).
       *
       * \throws SodiumInvalidCipher if the provided message was empty or too short
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if one of the provided key was empty
       *
       * \returns a heap-allocated buffer with the decrypted data or an invalid, empty buffer if the decryption failed
       */
      SodiumSecureMemory box_open_detached(
          const MemView& cipher,   ///< a memory buffer containing the cipher text
          const AsymCrypto_Tag& mac,   ///< a memory buffer containing the message authentication tag
          const AsymCrypto_Nonce& nonce,   ///< the nonce to be used for this specific message
          const AsymCrypto_PublicKey& senderKey,   ///< the sender's public key for verifying the message integrity
          const AsymCrypto_SecretKey& recipientKey,   ///< the recipient's secret key for decrypting the message
          SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked   ///< the protection class for the plain message
          );

      /** \brief Authenticated, public-key encryption in combined mode;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/authenticated_encryption.html).
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if one of the provided key was empty
       *
       * \returns a string with the encrypted data and an attached authentication tag
       */
      string box_easy(
          const string& msg,   ///< the plain input message
          const AsymCrypto_Nonce& nonce,   ///< the nonce to be used for this specific message
          const AsymCrypto_PublicKey& recipientKey,   ///< the recipient's public key
          const AsymCrypto_SecretKey& senderKey   ///< the sender's private key (for the authentication signature)
          );

      /** \brief Authenticated, public-key decryption in combined mode;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/authenticated_encryption.html).
       *
       * \throws SodiumInvalidCipher if the provided message was empty or too short
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if one of the provided key was empty
       *
       * \returns a string containing the decrypted data or an empty string if the decryption failed
       */
      string box_open_easy(
          const string& cipher,   ///< a string containing the cipher and the attached authentication tag
          const AsymCrypto_Nonce& nonce,   ///< the nonce to be used for this specific message
          const AsymCrypto_PublicKey& senderKey,   ///< the sender's public key for verifying the message integrity
          const AsymCrypto_SecretKey& recipientKey   ///< the recipient's secret key for decrypting the message
          );

      /** \brief Authenticated, public-key encryption in detached mode;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/authenticated_encryption.html).
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if one of the provided key was empty
       *
       * \returns a pair of a string with the encrypted data and a heap-allocated authentication tag
       */
      pair<string, AsymCrypto_Tag> box_detached(
          const string& msg,   ///< the plain input message
          const AsymCrypto_Nonce& nonce,   ///< the nonce to be used for this specific message
          const AsymCrypto_PublicKey& recipientKey,   ///< the recipient's public key
          const AsymCrypto_SecretKey& senderKey   ///< the sender's private key (for the authentication signature)
          );

      /** \brief Authenticated, public-key decryption in detached mode;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/authenticated_encryption.html).
       *
       * \throws SodiumInvalidCipher if the provided message was empty or too short
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if one of the provided key was empty
       *
       * \returns a string containing the decrypted data or an empty string if the decryption failed
       */
      string box_open_detached(
          const string& cipher,   ///< the encrypted cipher text
          const AsymCrypto_Tag& mac,   ///< a memory buffer containing the message authentication tag
          const AsymCrypto_Nonce& nonce,   ///< the nonce to be used for this specific message
          const AsymCrypto_PublicKey& senderKey,   ///< the sender's public key for verifying the message integrity
          const AsymCrypto_SecretKey& recipientKey   ///< the recipient's secret key for decrypting the message
          );

      // signatures, key handling
      using AsymSign_PublicKey = SodiumKey<SodiumKeyType::Public, crypto_sign_PUBLICKEYBYTES>;
      using AsymSign_SecretKey = SodiumKey<SodiumKeyType::Secret, crypto_sign_SECRETKEYBYTES>;
      using AsymSign_KeySeed = SodiumKey<SodiumKeyType::Public, crypto_sign_SEEDBYTES>;
      using AsymSign_Signature = SodiumKey<SodiumKeyType::Public, crypto_sign_BYTES>;

      /** \brief Generates a new, random public / private key pair for public key signatures;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/public-key_signatures.html).
       *
       * \returns `true` if the provided buffers could be filled with the key data; `false` otherwise.
       */
      bool genAsymSignKeyPair(
          AsymSign_PublicKey& pk_out,   ///< reference to an externally created buffer that will be filled with the public key data
          AsymSign_SecretKey& sk_out   ///< reference to an externally created buffer that will be filled with the secret key data
          );

      /** \brief Generates a new, seeded public / private key pair for public key signatures;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/public-key_signatures.html).
       *
       * \returns `true` if the provided buffers could be filled with the key data; `false` otherwise.
       */
      bool genAsymSignKeyPairSeeded(
          const AsymSign_KeySeed& seed,   ///< the seed to be used for the key generation
          AsymSign_PublicKey& pk_out,   ///< reference to an externally created buffer that will be filled with the public key data
          AsymSign_SecretKey& sk_out   ///< reference to an externally created buffer that will be filled with the secret key data
          );

      /** \brief Computes the public key for asymetric signatures from the associated secret key;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/public-key_signatures.html).
       *
       * \returns `true` if the provided buffer could be filled with the key data; `false` otherwise.
       */
      bool genPublicSignKeyFromSecretKey(
          const AsymSign_SecretKey& sk,   ///< the secret key from which to derive the public key
          AsymSign_PublicKey& pk_out   ///< reference to an externally created buffer that will be filled with the public key data
          );

      /** \brief Computes the seed that has been used for the generation signature key pair from the associated secret key;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/public-key_signatures.html).
       *
       * \returns `true` if the provided buffer could be filled with the key data; `false` otherwise.
       */
      bool genSignKeySeedFromSecretKey(
          const AsymSign_SecretKey& sk,   ///< the secret signature key
          AsymSign_KeySeed& seed_out   ///< reference to an externally created buffer that will be filled with the seed data
          );

      /** \brief Signes a messages using the private key of an asymetric key pair;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/public-key_signatures.html).
       *
       * \throws SodiumInvalidMessage if the provided message is empty
       *
       * \throws SodiumInvalidKey if the provided key is empty
       *
       * \returns a heap-allocated buffer containing the signature followed by a copy of the input message
       */
      MemArray sign(
          const MemView& msg,   ///< the input message for the signature
          const AsymSign_SecretKey& sk   ///< the secret key for signing the message
          );

      /** \brief Checks the signature of a messages using the public key of an asymetric key pair;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/public-key_signatures.html).
       *
       * \throws SodiumInvalidMessage if the provided message is empty or too short
       *
       * \throws SodiumInvalidKey if the provided key is empty
       *
       * \returns a heap-allocated buffer containing the message if the signature was successfully verified; an empty buffer otherwise
       */
      MemArray sign_open(
          const MemView& signedMsg,   ///< a memory buffer containing the signature followed by the message itself
          const AsymSign_PublicKey& pk   ///< the public key for checking the signature
          );

      /** \brief Signes a messages using the private key of an asymetric key pair;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/public-key_signatures.html).
       *
       * \throws SodiumInvalidMessage if the provided message is empty
       *
       * \throws SodiumInvalidKey if the provided key is empty
       *
       * \returns a heap-allocated buffer containing the signature
       */
      AsymSign_Signature sign_detached(
          const MemView& msg,   ///< the input message for the signature
          const AsymSign_SecretKey& sk   ///< the secret key for signing the message
          );

      /** \brief Checks the signature of a messages using the public key of an asymetric key pair;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/public-key_signatures.html).
       *
       * \throws SodiumInvalidMessage if the provided message is empty
       *
       * \throws SodiumInvalidMac if the provided signature is empty
       *
       * \throws SodiumInvalidKey if the provided key is empty
       *
       * \returns `true` if the message could be verified by the signature; `false` otherwise
       */
      bool sign_verify_detached(
          const MemView& msg,   ///< the message that shall be verified
          const AsymSign_Signature& sig,   ///< the signature to be used for the verification
          const AsymSign_PublicKey& pk   ///< the public key for checking the signature
          );

      /** \brief Signes a messages using the private key of an asymetric key pair;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/public-key_signatures.html).
       *
       * \throws SodiumInvalidMessage if the provided message is empty
       *
       * \throws SodiumInvalidKey if the provided key is empty
       *
       * \returns a string containing the signature followed by a copy of the input message
       */
      string sign(
          const string& msg,   ///< the input message for the signature
          const AsymSign_SecretKey& sk   ///< the secret key for signing the message
          );

      /** \brief Checks the signature of a messages using the public key of an asymetric key pair;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/public-key_signatures.html).
       *
       * \throws SodiumInvalidMessage if the provided message is empty or too short
       *
       * \throws SodiumInvalidKey if the provided key is empty
       *
       * \returns a string containing the message if the signature was successfully verified; an empty string otherwise
       */
      string sign_open(
          const string& signedMsg,   ///< a string containing the signature followed by the message itself
          const AsymSign_PublicKey& pk   ///< the public key for checking the signature
          );

      /** \brief Signes a messages using the private key of an asymetric key pair;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/public-key_signatures.html).
       *
       * \throws SodiumInvalidMessage if the provided message is empty
       *
       * \throws SodiumInvalidKey if the provided key is empty
       *
       * \returns a string containing the signature
       */
      AsymSign_Signature sign_detached(
          const string& msg,   ///< the input message for the signature
          const AsymSign_SecretKey& sk   ///< the secret key for signing the message
          );

      /** \brief Checks the signature of a messages using the public key of an asymetric key pair;
       * original documentation [here](https://download.libsodium.org/doc/public-key_cryptography/public-key_signatures.html).
       *
       * \throws SodiumInvalidMessage if the provided message is empty
       *
       * \throws SodiumInvalidMac if the provided signature is empty
       *
       * \throws SodiumInvalidKey if the provided key is empty
       *
       * \returns `true` if the message could be verified by the signature; `false` otherwise
       */
      bool sign_verify_detached(const string& msg, const AsymSign_Signature& sig, const AsymSign_PublicKey& pk);

      // hashing
      using GenericHashKey = SodiumKey<SodiumKeyType::Secret, crypto_generichash_KEYBYTES>;
      using ShorthashKey = SodiumKey<SodiumKeyType::Secret, crypto_shorthash_KEYBYTES>;

      /** \brief Computes a fingerprint for a given memory buffer;
       * original documentation [here](https://download.libsodium.org/doc/hashing/generic_hashing.html).
       *
       * The hash size can be anything between and including
       * `crypto_generichash_BYTES_MIN` (currently 16 bytes / 128 bit) and
       * `crypto_generichash_BYTES_MAX` (64 bytes / 512 bit) and
       * is recommended to be `crypto_generichash_BYTES` (32 bytes / 256 bit).
       *
       * \warning This hashing function is **not suitable for password hashing** because
       * is not sufficiently computation- and memory-heavy!
       *
       * \throws SodiumInvalidMessage if the provided data is empty
       *
       * \throws std::range_error if the provided hash length is out of range
       *
       * \returns a heap-allocated buffer containg the hash data
       */
      MemArray generichash(
          const MemView& inData,   ///< the data that is to be hashed
          size_t hashLen = crypto_generichash_BYTES   ///< the size of the resulting hash in bytes
          );

      /** \brief Computes a fingerprint for a given memory buffer;
       * original documentation [here](https://download.libsodium.org/doc/hashing/generic_hashing.html).
       *
       * The hash size can be anything between and including
       * `crypto_generichash_BYTES_MIN` (currently 16 bytes / 128 bit) and
       * `crypto_generichash_BYTES_MAX` (64 bytes / 512 bit) and
       * is recommended to be `crypto_generichash_BYTES` (32 bytes / 256 bit).
       *
       * \warning This hashing function is **not suitable for password hashing** because
       * is not sufficiently computation- and memory-heavy!
       *
       * \throws SodiumInvalidMessage if the provided data is empty
       *
       * \throws SodiumInvalidKey if the provided key is empty
       *
       * \throws std::range_error if the provided hash length is out of range
       *
       * \returns a heap-allocated buffer containg the hash data
       */
      MemArray generichash(
          const MemView& inData,   ///< the data that is to be hashed
          const GenericHashKey& key,   ///< the key that shall be used for the hash computation
          size_t hashLen = crypto_generichash_BYTES   ///< the size of the resulting hash in bytes
          );

      /** \brief Computes a fingerprint for a given string;
       * original documentation [here](https://download.libsodium.org/doc/hashing/generic_hashing.html).
       *
       * The hash size can be anything between and including
       * `crypto_generichash_BYTES_MIN` (currently 16 bytes / 128 bit) and
       * `crypto_generichash_BYTES_MAX` (64 bytes / 512 bit) and
       * is recommended to be `crypto_generichash_BYTES` (32 bytes / 256 bit).
       *
       * \warning This hashing function is **not suitable for password hashing** because
       * is not sufficiently computation- and memory-heavy!
       *
       * \throws SodiumInvalidMessage if the provided data is empty
       *
       * \throws std::range_error if the provided hash length is out of range
       *
       * \returns a string containg the hash data
       */
      string generichash(
          const string& inData,    ///< the data that is to be hashed
          size_t hashLen = crypto_generichash_BYTES   ///< the size of the resulting hash in bytes
          );

      /** \brief Computes a fingerprint for a given string;
       * original documentation [here](https://download.libsodium.org/doc/hashing/generic_hashing.html).
       *
       * The hash size can be anything between and including
       * `crypto_generichash_BYTES_MIN` (currently 16 bytes / 128 bit) and
       * `crypto_generichash_BYTES_MAX` (64 bytes / 512 bit) and
       * is recommended to be `crypto_generichash_BYTES` (32 bytes / 256 bit).
       *
       * \warning This hashing function is **not suitable for password hashing** because
       * is not sufficiently computation- and memory-heavy!
       *
       * \throws SodiumInvalidMessage if the provided data is empty
       *
       * \throws SodiumInvalidKey if the provided key is empty
       *
       * \throws std::range_error if the provided hash length is out of range
       *
       * \returns a string containg the hash data
       */
      string generichash(
          const string& inData,
          const GenericHashKey& key,   ///< the key that shall be used for the hash computation
          size_t hashLen = crypto_generichash_BYTES   ///< the size of the resulting hash in bytes
          );

      /** \brief Initializes a hashing operation with multiple data chunks;
       * original documentation [here](https://download.libsodium.org/doc/hashing/generic_hashing.html).
       *
       * The hash size can be anything between and including
       * `crypto_generichash_BYTES_MIN` (currently 16 bytes / 128 bit) and
       * `crypto_generichash_BYTES_MAX` (64 bytes / 512 bit) and
       * is recommended to be `crypto_generichash_BYTES` (32 bytes / 256 bit).
       *
       * \warning This hashing function is **not suitable for password hashing** because
       * is not sufficiently computation- and memory-heavy!
       *
       * \throws std::invalid_argument if the provided pointer to a hash state variable is `nullptr'
       *
       * \throws std::range_error if the provided hash length is out of range
       */
      void generichash_init(
          crypto_generichash_state *state,   ///< pointer to an externally owned hash state variable
          size_t hashLen = crypto_generichash_BYTES   ///< the size of the resulting hash in bytes
          );

      /** \brief Initializes a hashing operation with multiple data chunks;
       * original documentation [here](https://download.libsodium.org/doc/hashing/generic_hashing.html).
       *
       * The hash size can be anything between and including
       * `crypto_generichash_BYTES_MIN` (currently 16 bytes / 128 bit) and
       * `crypto_generichash_BYTES_MAX` (64 bytes / 512 bit) and
       * is recommended to be `crypto_generichash_BYTES` (32 bytes / 256 bit).
       *
       * \warning This hashing function is **not suitable for password hashing** because
       * is not sufficiently computation- and memory-heavy!
       *
       * \throws std::invalid_argument if the provided pointer to a hash state variable is `nullptr'
       *
       * \throws SodiumInvalidKey if the provided key is empty
       *
       * \throws std::range_error if the provided hash length is out of range
       */
      void generichash_init(
          crypto_generichash_state *state,   ///< pointer to an externally owned hash state variable
          const GenericHashKey& key,   ///< the key that shall be used for the hash computation
          size_t hashLen = crypto_generichash_BYTES   ///< the size of the resulting hash in bytes
          );

      /** \brief Hashes a chunk of data as part of a multi-chunk hashing operation;
       * original documentation [here](https://download.libsodium.org/doc/hashing/generic_hashing.html).
       *
       * Prior to calling this function, the hashing state has to be initialized
       * using `crypto_generichash_init()`.
       *
       * \throws std::invalid_argument if the provided pointer to a hash state variable is `nullptr'
       *
       * \throws SodiumInvalidMessage if the provided data is empty
       */
      void generichash_update(
          crypto_generichash_state *state,   ///< pointer to an externally owned hash state variable
          const MemView& inData   ///< the data that shall be hashed
          );

      /** \brief Hashes a chunk of data as part of a multi-chunk hashing operation;
       * original documentation [here](https://download.libsodium.org/doc/hashing/generic_hashing.html).
       *
       * Prior to calling this function, the hashing state has to be initialized
       * using `crypto_generichash_init()`.
       *
       * \throws std::invalid_argument if the provided pointer to a hash state variable is `nullptr'
       *
       * \throws SodiumInvalidMessage if the provided data is empty
       */
      void generichash_update(
          crypto_generichash_state *state,   ///< pointer to an externally owned hash state variable
          const string& inData   ///< the data that shall be hashed
          );

      /** \brief Finalizes a multi-chunk hashing operation and returns the hash;
       * original documentation [here](https://download.libsodium.org/doc/hashing/generic_hashing.html).
       *
       * \note The `hashLen` should be the same that been provided to `crypto_generichash_init()`.
       *
       * \throws std::invalid_argument if the provided pointer to a hash state variable is `nullptr'
       *
       * \throws std::range_error if the provided hash length is out of range
       *
       * \returns a heap-allocated buffer containg the hash data
       */
      MemArray generichash_final(
          crypto_generichash_state *state,   ///< pointer to an externally owned hash state variable
          size_t hashLen = crypto_generichash_BYTES   ///< the size of the resulting hash in bytes
          );

      /** \brief Finalizes a multi-chunk hashing operation and returns the hash;
       * original documentation [here](https://download.libsodium.org/doc/hashing/generic_hashing.html).
       *
       * \note The `hashLen` should be the same that been provided to `crypto_generichash_init()`.
       *
       * \throws std::invalid_argument if the provided pointer to a hash state variable is `nullptr'
       *
       * \throws std::range_error if the provided hash length is out of range
       *
       * \returns a heap-string containg the hash data
       */
      string generichash_final_string(
          crypto_generichash_state *state,   ///< pointer to an externally owned hash state variable
          size_t hashLen = crypto_generichash_BYTES   ///< the size of the resulting hash in bytes
          );

      /** \brief Computes a short, 64-bit hash, e.g., for building hash tables;
       * original documentation [here](https://download.libsodium.org/doc/hashing/short-input_hashing.html).
       *
       * The key has to be initialized by the user by using a stored value or by
       * filling it with random data, for instance.
       *
       * The key has to remain secret (see libsodium docs for details).
       *
       * \throws SodiumInvalidMessage if the provided data is empty
       *
       * \throws SodiumInvalidKey if the provided key is empty
       *
       * \returns a heap-allocated buffer containg the hash data
       */
      MemArray shorthash(
          const MemView& inData,   ///< the data that shall be hashed
          const ShorthashKey& key    ///< a mandatory, fixed size key
          );

      /** \brief Computes a short, 64-bit hash, e.g., for building hash tables;
       * original documentation [here](https://download.libsodium.org/doc/hashing/short-input_hashing.html).
       *
       * The key has to be initialized by the user by using a stored value or by
       * filling it with random data, for instance.
       *
       * The key has to remain secret (see libsodium docs for details).
       *
       * \throws SodiumInvalidMessage if the provided data is empty
       *
       * \throws SodiumInvalidKey if the provided key is empty
       *
       * \returns a string containg the hash data
       */
      string shorthash(
          const string& inData,   ///< the data that shall be hashed
          const ShorthashKey& key    ///< a mandatory, fixed size key
          );

      // password hashing and key derivation
      using Argon_Salt = SodiumKey<SodiumKeyType::Public, crypto_pwhash_SALTBYTES>;
      enum class PasswdHashStrength
      {
        Interactive,
        Moderate,
        High
      };

      enum class PasswdHashAlgo
      {
        Default,   ///< libsodium's default algorithm (currently Argon2id)
        Argon2i,   ///< Argon2i (multiple-pass, iterative Argon2)
        Argon2id   ///< Argon2id (iterative combined with data-independant memory useage
      };
      int PasswdHashAlgo2Int(PasswdHashAlgo alg)
      {
        if (alg == PasswdHashAlgo::Argon2id) return crypto_pwhash_ALG_ARGON2ID13;
        if (alg == PasswdHashAlgo::Argon2i) return crypto_pwhash_ALG_ARGON2I13;
        return crypto_pwhash_ALG_DEFAULT;
      }

      struct PwHashData
      {
        Argon_Salt salt;   ///< the salt for the password hash
        unsigned long long opslimit;   ///< the number of hashing iterations (e.g., 3)
        size_t memlimit;   ///< the amount of memory to be used by the hashing algo in bytes
        PasswdHashAlgo algo;   ///< the selected hashing algorithm
      };

      /** \brief Computes a memory- and CPU-hard hash value from a given clear text password;
       * original documentation [here](https://download.libsodium.org/doc/password_hashing)
       * and especially [here](https://download.libsodium.org/doc/password_hashing/the_argon2i_function.html).
       *
       * Can also be used to derive keys from passwords.
       *
       * The password length should be between `crypto_pwhash_PASSWD_MIN` and
       * `crypto_pwhash_PASSWD_MAX`.
       *
       * The length of the resulting hash can be between
       * `crypto_pwhash_BYTES_MIN' (currently 16 bytes / 128 bits) and
       * `crypto_pwhash_BYTES_MAX` ().
       *
       * \throws SodiumInvalidBuffer if the clear text password has no valid length
       *
       * \throws std::range_error if the requested hash length is invalid
       *
       * \returns a pair of the hash itself and the actual, numeric parameters used for its computation;
       * in case of libsodium-internal errors, the resulting hash is empty!
       */
      pair<SodiumSecureMemory, PwHashData> pwhash(
          const MemView& pw,   ///< a memory buffer containing the plain text password
          size_t hashLen,   ///< the number of bytes for the resulting hash
          PasswdHashStrength strength = PasswdHashStrength::Moderate,   ///< the required complexity (--> security) for the hashing
          PasswdHashAlgo algo = PasswdHashAlgo::Default,   ///< the algorithm to be used for the hashing
          SodiumSecureMemType memType = SodiumSecureMemType::Locked   ///< the protection class for the resulting hash / key
          );

      /** \brief Computes a memory- and CPU-hard hash value from a given clear text password;
       * original documentation [here](https://download.libsodium.org/doc/password_hashing)
       * and especially [here](https://download.libsodium.org/doc/password_hashing/the_argon2i_function.html).
       *
       * Can also be used to derive keys from passwords.
       *
       * \note The salt part of PwHashData **has to be initialized by the caller** before
       * calling this function. 'randombytes_buf()' is a good candidate for that purpose.
       *
       * The length of the resulting hash can be between
       * `crypto_pwhash_BYTES_MIN' (currently 16 bytes / 128 bits) and
       * `crypto_pwhash_BYTES_MAX` (4294967295 bytes).
       *
       * \throws SodiumInvalidBuffer if the clear text password is empty
       *
       * \throws std::range_error if the requested hash length is invalid
       *
       * \returns the hash; in case of libsodium-internal errors, the resulting hash is empty!
       */
      SodiumSecureMemory pwhash(
          const MemView& pw,   ///< a memory buffer containing the plain text password
          size_t hashLen,   ///< the number of bytes for the resulting hash
          PwHashData& hDat,   ///< the numeric hashing parameters
          SodiumSecureMemType memType = SodiumSecureMemType::Locked   ///< the protection class for the resulting hash / key
          );

      /** \brief Computes a memory- and CPU-hard hash value from a given clear text password and stores the hash
       * along with its computation parameters in an ASCII-only string, e.g. for storage in a database;
       * original documentation [here](https://download.libsodium.org/doc/password_hashing)
       * and especially [here](https://download.libsodium.org/doc/password_hashing/the_argon2i_function.html).
       *
       * The resulting string can be used for password verification using `crypto_pwhash_str_verify()`.
       *
       * \throws SodiumInvalidBuffer if the clear text password is empty
       *
       * \returns an ASCII-only string that contains the hash and its computation parameters;
       * if libsodium failed to compute the string, the string is empty.
       */
      string pwhash_str(
          const MemView& pw,   ///< a memory buffer containing the plain text password
          PasswdHashStrength strength = PasswdHashStrength::Moderate   ///< the required complexity (--> security) for the hashing
          );

      /** \brief Computes a memory- and CPU-hard hash value from a given clear text password and stores the hash
       * along with its computation parameters in an ASCII-only string, e.g. for storage in a database;
       * original documentation [here](https://download.libsodium.org/doc/password_hashing)
       * and especially [here](https://download.libsodium.org/doc/password_hashing/the_argon2i_function.html).
       *
       * The resulting string can be used for password verification using `crypto_pwhash_str_verify()`.
       *
       * \throws SodiumInvalidBuffer if the clear text password is empty
       *
       * \returns an ASCII-only string that contains the hash and its computation parameters;
       * if libsodium failed to compute the string, the string is empty.
       */
      string pwhash_str(
          const string& pw,   ///< a string containing the plain text password
          PasswdHashStrength strength = PasswdHashStrength::Moderate   ///< the required complexity (--> security) for the hashing
          );

      /** \brief Verifies a given clear text password against a previously computed password hash;
       * original documentation [here](https://download.libsodium.org/doc/password_hashing)
       * and especially [here](https://download.libsodium.org/doc/password_hashing/the_argon2i_function.html).
       *
       * The hash string has to be generated using `crypto_pwhash_str()` in order to include
       * all necessary computation parameters.
       *
       * \returns `true` if the password verification succeeded; `false` if it failed
       * because the password was wrong or because an error occurred.
       */
      bool pwhash_str_verify(
          const MemView& pw,   ///< the plain text password that shall be verified
          const string& hashResult   ///< the hash of the "true" password computed via `crypto_pwhash_str()`
          );

      /** \brief Verifies a given clear text password against a previously computed password hash;
       * original documentation [here](https://download.libsodium.org/doc/password_hashing)
       * and especially [here](https://download.libsodium.org/doc/password_hashing/the_argon2i_function.html).
       *
       * The hash string has to be generated using `crypto_pwhash_str()` in order to include
       * all necessary computation parameters.
       *
       * \returns `true` if the password verification succeeded; `false` if it failed
       * because the password was wrong or because an error occurred.
       */
      bool pwhash_str_verify(
          const string& pw,   ///< the plain text password that shall be verified
          const string& hashResult   ///< the hash of the "true" password computed via `crypto_pwhash_str()`
          );

      /** \brief Translates the enums in `PasswdHashStrength' into numerical
       * values for `opslimit` and `memlimit' based on the constants defined by libsodium.
       *
       * \returns a pair of `opslimit` and `memlimit` values for the given hash strength
       */
      pair<unsigned long long, size_t> pwHashConfigToValues(PasswdHashStrength strength);

      // Diffie-Hellmann key exchange
      using DH_PublicKey = SodiumKey<SodiumKeyType::Public, crypto_scalarmult_BYTES>;
      using DH_SecretKey = SodiumKey<SodiumKeyType::Secret, crypto_scalarmult_SCALARBYTES>;
      using DH_SharedSecret = SodiumKey<SodiumKeyType::Secret, crypto_scalarmult_BYTES>;

      /** \brief Generates a public / private key pair for the Diffie-Hellmann key exchange;
       * original documentation [here](https://download.libsodium.org/doc/advanced/scalar_multiplication.html)
       *
       * The secret key is initialized from random data.
       *
       * \note This call uses a deprecated API of libsodium; for new implementations,
       * use the 'crypto_kx_*'-API instead.
       *
       * \returns a pair of secret and public key for DH key exchange
       */
      pair<DH_SecretKey, DH_PublicKey> genDHKeyPair();

      /** \brief Computes the shared secret for the Diffie-Hellmann key exchange;
       * original documentation [here](https://download.libsodium.org/doc/advanced/scalar_multiplication.html)
       *
       * \note This call uses a deprecated API of libsodium; for new implementations,
       * use the 'crypto_kx_*'-API instead.
       *
       * \throws SodiumInvalidKey if either the secret and/or the public key are empty
       *
       * \returns The resulting shared secret between the key exchange parties
       */
      DH_SharedSecret genDHSharedSecret(
          const DH_SecretKey& mySecretKey,   ///< the secret key of the local peer
          const DH_PublicKey& othersPublicKey   ///< the public key of the remote peer
          );

      /** \brief Computes the public key that belongs to a secret key for the Diffie-Hellmann key exchange;
       * original documentation [here](https://download.libsodium.org/doc/advanced/scalar_multiplication.html)
       *
       * \note This call uses a deprecated API of libsodium; for new implementations,
       * use the 'crypto_kx_*'-API instead.
       *
       * \throws SodiumInvalidKey if the secret key is empty
       *
       * \returns The public key that belongs to a secret key for the DH key exchange
       */
      DH_PublicKey genPublicDHKeyFromSecretKey(const DH_SecretKey& sk);

      //
      // New key exchange functions introduced in libSodium 1.0.12
      //
      using KX_SessionKey = SodiumKey<SodiumKeyType::Secret, crypto_kx_SESSIONKEYBYTES>;
      using KX_PublicKey = SodiumKey<SodiumKeyType::Public, crypto_kx_PUBLICKEYBYTES>;
      using KX_SecretKey = SodiumKey<SodiumKeyType::Secret, crypto_kx_SECRETKEYBYTES>;
      using KX_KeySeed = SodiumKey<SodiumKeyType::Secret, crypto_kx_SEEDBYTES>;

      /** \brief Generates a random public / private key pair for the new crypto_kx_* key
       * exchange functions;
       * original documentation [here](https://download.libsodium.org/doc/key_exchange)
       *
       * \returns a pair of secret and public key for client/server key exchange
       */
      pair<KX_SecretKey, KX_PublicKey> genKeyExchangeKeyPair();

      /** \brief Generates a deterministic public / private key pair for the new crypto_kx_* key
       * exchange functions based on a given seed;
       * original documentation [here](https://download.libsodium.org/doc/key_exchange)
       *
       * \throws SodiumInvalidKey if the provided seed was empty
       *
       * \returns a pair of secret and public key for client/server key exchange
       */
      pair<KX_SecretKey, KX_PublicKey> genKeyExchangeKeyPair(
          const KX_KeySeed& seed   ///< seed to be used for key generation, ready/unlocked for reading
          );

      /** \brief Generates the client's session key pair from the client's
       * public/secret keys and the server's public key;
       * original documentation [here](https://download.libsodium.org/doc/key_exchange)
       *
       * \note The `tx` key should be used to transmit data from the client
       * to the server and the 'rx' key should be used in the other direction.
       *
       * \throws SodiumInvalidKey if one of the provided keys was empty or
       * the public key invalid.
       *
       * \returns a pair of 'rx` and `tx` session keys
       */
      pair<KX_SessionKey, KX_SessionKey> getClientSessionKeys(
          const KX_PublicKey& clientPubKey,   ///< the client's public key
          const KX_SecretKey& clientSecKey,   ///< the client's secret key, ready/unlocked for reading
          const KX_PublicKey& serverPubKey    ///< the server's public key
          );

      /** \brief Generates the server's session key pair from the server's
       * public/secret keys and the client's public key;
       * original documentation [here](https://download.libsodium.org/doc/key_exchange)
       *
       * \note The `tx` key should be used to transmit data from the server
       * to the client and the 'rx' key should be used in the other direction.
       *
       * \throws SodiumInvalidKey if one of the provided keys was empty or
       * the public key invalid.
       *
       * \returns a pair of 'rx` and `tx` session keys
       */
      pair<KX_SessionKey, KX_SessionKey> getServerSessionKeys(
          const KX_PublicKey& serverPubKey,   ///< the server's public key
          const KX_SecretKey& serverSecKey,   ///< the server's secret key, ready/unlocked for reading
          const KX_PublicKey& clientPubKey   ///< the client's public key
          );

    protected:
      /** \brief Ctor for the lib wrapper; loads all necessary symbols / function pointers
       * from the lib at runtime.
       *
       * The ctor is only called once from the static `getInstance()` function when the
       * singleton is initialized.
       */
      SodiumLib(
          void* _libHandle   ///< handle to the dynamically loaded libsodium as returned by `dlopen()`
          );

      static unique_ptr<SodiumLib> inst;   ///< the singleton instance of the lib wrapper

      void *libHandle;      ///< handle to the dynamically loaded libsodium as returned by `dlopen()`

      SodiumPtr sodium;   ///< a table of function pointers for libsodium calls

      /** \brief An internal wrapper for providing a harmonized interface for 'secretbox_open_easy'-calls (combined mode).
       *
       * \throws SodiumInvalidCipher if the provided cipher text is empty or too short
       *
       * \throws SodiumInvalidNonce if the provided nonce is empty
       *
       * \throws SodiumInvalidKey if the provided secret key is empty
       *
       * \throws SodiumInvalidBuffer if the provided target data buffer is empty or too small
       *
       * \returns `true` if the decryption call succeeded
       */
      bool secretbox_open_easy__internal(
          const MemArray& targetBuf,   ///< the target buffer for the decrypted data
          const MemView& cipher,   ///< the encrypted data including authentication MAC
          const SecretBoxNonce& nonce,   ///< the nonce for this specific message
          const SecretBoxKey& key   ///< the secret key used for encryption / decryption
          );

      /** \brief An internal, generic handler for AEAD encryption in combined mode
       *
       * Takes a function pointer to the actual encryption function (e.g, for
       * XChaCha20 or AES256-GCM) and a generic set of encryption parameters.
       *
       * \note In case the actual tag is shorter than the provided tag size, a
       * call to this function involves an additional, internal copying of the
       * encrypted message and its tag into a correctly sized buffer (==> performance).
       *
       * \warning It is the caller's responsibility that the provided key and nonce
       * arrays are of correct size for the selected encryption function! Otherwise,
       * reads beyond the actual buffer end may occur!
       *
       * \throws invalid_argument if the provided function pointer was empty
       *
       * \throws SodiumInvalidMac if the provided tag size was zero
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \returns a string with the combined encrypted data and authentication tag
       */
      string aead_encrypt(
          int (*funcPtr)(unsigned char *, unsigned long long *, const unsigned char *,
                         unsigned long long, const unsigned char *, unsigned long long,
                         const unsigned char *, const unsigned char *,const unsigned char *),   ///< a pointer to the actual encryption function
          size_t tagSize,   ///< the number of MAC bytes computed by the encryption function
          const string& msg,   ///< the input message for encryption
          const MemView& nonce,   ///< the nonce to be used for this specific message
          const MemView& key,   ///< the secret key; the caller has to ensure that the memory is enabled for reading
          const string& ad = string{}   ///< optional, additional data to be included in the MAC computation
          );

      /** \brief An internal, generic handler for AEAD encryption in combined mode
       *
       * Takes a function pointer to the actual encryption function (e.g, for
       * XChaCha20 or AES256-GCM) and a generic set of encryption parameters.
       *
       * \note In case the actual tag is shorter than the provided tag size, a
       * call to this function involves an additional, internal copying of the
       * encrypted message and its tag into a correctly sized buffer (==> performance).
       *
       * \warning It is the caller's responsibility that the provided key and nonce
       * arrays are of correct size for the selected encryption function! Otherwise,
       * reads beyond the actual buffer end may occur!
       *
       * \throws invalid_argument if the provided function pointer was empty
       *
       * \throws SodiumInvalidMac if the provided tag size was zero
       *
       * \throws SodiumInvalidMessage if the provided message was empty
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \returns a heap-allocated buffer with the combined encrypted data
       * and authentication tag
       */
      MemArray aead_encrypt(
          int (*funcPtr)(unsigned char *, unsigned long long *, const unsigned char *,
                         unsigned long long, const unsigned char *, unsigned long long,
                         const unsigned char *, const unsigned char *,const unsigned char *),   ///< a pointer to the actual encryption function
          size_t tagSize,   ///< the number of MAC bytes computed by the encryption function
          const MemView& msg,   ///< the input message for encryption
          const MemView& nonce,   ///< the nonce to be used for this specific message
          const MemView& key,   ///< the secret key; the caller has to ensure that the memory is enabled for reading
          const MemView& ad = MemView{}   ///< optional, additional data to be included in the MAC computation
          );

      /** \brief An internal, generic handler for AEAD decryption in combined mode
       *
       * Takes a function pointer to the actual decryption function (e.g, for
       * XChaCha20 or AES256-GCM) and a generic set of decryption parameters.
       *
       * \note In case the actual tag is shorter than the provided tag size, a
       * call to this function involves an additional, internal copying of the
       * decrypted message into a correctly sized buffer (==> performance).
       *
       * \warning It is the caller's responsibility that the provided key and nonce
       * arrays are of correct size for the selected decryption function! Otherwise,
       * reads beyond the actual buffer end may occur!
       *
       * \throws invalid_argument if the provided function pointer was empty
       *
       * \throws SodiumInvalidMac if the provided tag size was zero
       *
       * \throws SodiumInvalidCipher if the provided message was empty or too short
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \returns a heap-allocated buffer with the combined encrypted data
       * and authentication tag
       */
      SodiumSecureMemory aead_decrypt(
          int (*funcPtr)(unsigned char *, unsigned long long *, unsigned char *,
                         const unsigned char *, unsigned long long, const unsigned char *,
                         unsigned long long, const unsigned char *, const unsigned char *),
          size_t tagSize,   ///< the number of MAC bytes computed by the encryption function
          const MemView& cipher,   ///< the combined cipher and authentication tag
          const MemView& nonce,   ///< the nonce used for this specific message
          const MemView& key,   ///< the secret key; the caller has to ensure that the memory is enabled for reading
          const MemView& ad = MemView{},   ///< optional, additional data to be included in the MAC computation
          SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked   ///< the protection class of the unencrypted, plain data
          );


      /** \brief An internal, generic handler for AEAD decryption in combined mode
       *
       * Takes a function pointer to the actual decryption function (e.g, for
       * XChaCha20 or AES256-GCM) and a generic set of decryption parameters.
       *
       * \note In case the actual tag is shorter than the provided tag size, a
       * call to this function involves an additional, internal copying of the
       * decrypted message into a correctly sized buffer (==> performance).
       *
       * \warning It is the caller's responsibility that the provided key and nonce
       * arrays are of correct size for the selected decryption function! Otherwise,
       * reads beyond the actual buffer end may occur!
       *
       * \throws invalid_argument if the provided function pointer was empty
       *
       * \throws SodiumInvalidMac if the provided tag size was zero
       *
       * \throws SodiumInvalidCipher if the provided message was empty or too short
       *
       * \throws SodiumInvalidNonce if the provided nonce was empty
       *
       * \throws SodiumInvalidKey if the provided key was empty
       *
       * \returns a heap-allocated buffer with the combined encrypted data
       * and authentication tag
       */
      string aead_decrypt(
          int (*funcPtr)(unsigned char *, unsigned long long *, unsigned char *,
                         const unsigned char *, unsigned long long, const unsigned char *,
                         unsigned long long, const unsigned char *, const unsigned char *),
          size_t tagSize,   ///< the number of MAC bytes computed by the encryption function
          const string& cipher,   ///< the combined cipher and authentication tag
          const MemView& nonce,   ///< the nonce used for this specific message
          const MemView& key,   ///< the secret key; the caller has to ensure that the memory is enabled for reading
          const string& ad = string{}   ///< optional, additional data to be included in the MAC computation
          );

    };

    /** \brief A template class that provides nonce handling for "crypto boxes"
     */
    template <typename NonceType>
    class NonceBox
    {
    public:
      /** \brief Ctor; initializes the internal nonce handling with an initial nonce
       *
       * \throws SodiumNotAvailableException if the SodiumLib could be not be initialized
       *
       * \throws SodiumInvalidNonce if the provided initial nonce is empty
       */
      NonceBox(
          const NonceType& _nonce   ///< the initial nonce
          )
        :initialNonce{_nonce}, prevNonce{_nonce}, curNonce{_nonce}, nonceIncrementCount{0}, lib{SodiumLib::getInstance()}
      {
        if (lib == nullptr)
        {
          throw SodiumNotAvailableException{};
        }
        if (_nonce.empty())
        {
          throw SodiumInvalidNonce("ctor NonceBox");
        }
      }

      /** \brief Increments the nonce by one.
       */
      void increment()
      {
        if (nonceIncrementCount > 0) lib->increment(prevNonce.toNotOwningArray());

        lib->increment(curNonce.toNotOwningArray());

        ++nonceIncrementCount;
      }

      /** \returns the nonce **before** the last increment operation or the initial nonce
       * if no increment took place so far.
       */
      NonceType getPrevNonce() const { return NonceType{prevNonce}; }

      /** \returns the a copy of the current nonce (e.g., after the last increment call)
       * or the initial nonce if no increment took place so far.
       */
      NonceType getNonce() const { return NonceType{curNonce}; }

      /** \returns the a read-only reference to the current nonce (e.g., after the last increment call)
       * or to the initial nonce if no increment took place so far.
       *
       * This avoids the copy operation employed by `getNonce()`.
       */
      const NonceType& getNonceAsRef() const { return curNonce; }

      /** \returns the number of calls to the `increment()` function
       */
      size_t getNonceIncrementCount() const { return nonceIncrementCount; }

      /** \brief Resets the nonce to its initial value as provided to the ctor
       * or as defined by calling `setNonce()`.
       */
      void resetNonce()
      {
        curNonce = initialNonce.copy();
        prevNonce = initialNonce.copy();
        nonceIncrementCount = 0;
      }

      /** \brief Sets the nonce to a provided value and also overrides the
       * initial nonce value that has been provided to the ctor.
       *
       * \throws SodiumInvalidNonce if the provided nonce is empty
       */
      void setNonce(const NonceType& n)
      {
        if (n.empty())
        {
          throw SodiumInvalidNonce("NonceBox::setNonce");
        }

        initialNonce = n.copy();
        resetNonce();
      }

    protected:
      NonceType initialNonce;
      NonceType prevNonce;
      NonceType curNonce;
      size_t nonceIncrementCount;
      SodiumLib* lib;
    };

    /** \brief A class for easy symmetric encryption / decryption including nonce handling and
     * automatic locking / unlocking of the memory that stores the secret key
     *
     * This class uses libsodium's secretbox-API.
     */
    class SodiumSecretBox
    {
    public:
      /** \brief Ctor for a new secret box.
       *
       * \throws SodiumNotAvailableException if the SodiumLib could be not be initialized
       *
       * \throws SodiumInvalidKey if the provided secret key is empty
       *
       * \throws SodiumInvalidNonce if the provided initial nonce is empty
       *
       * \throws SodiumMemoryGuardException if the internal copy of the secret key could not be locked
       */
      SodiumSecretBox(
          const SodiumLib::SecretBoxKey& _key,   ///< the shared, secret key for the symmetric encryption
          const SodiumLib::SecretBoxNonce& _nonce,   ///< the initial once that will be used for the first encryption / decryption operation
          bool autoIncNonce = false   ///< if set to `true` the nonce will be automatically incremented after each encryption / decryption operation
          );

      /** \brief Encrypts a memory buffer using the secret key and the current nonce.
       *
       * \throws SodiumInvalidMessage if the input buffer is empty.
       *
       * \returns a heap-allocated buffer containing the encrypted data and an message authentication tag.
       */
      MemArray encryptCombined(
          const MemView& msg   ///< the input data that shall be encrypted
          );

      /** \brief Encrypts a memory buffer using the secret key the current nonce.
       *
       * \throws SodiumInvalidMessage if the input buffer is empty.
       *
       * \returns a pair of a heap-allocated buffer containing the encrypted data and an message authentication tag.
       */
      pair<MemArray, SodiumLib::SecretBoxMac> encryptDetached(
          const MemView& msg   ///< the input data that shall be encrypted
          );

      /** \brief Encrypts a string using the secret key the current nonce.
       *
       * \throws SodiumInvalidMessage if the input string is empty.
       *
       * \returns a string containing the encrypted data and an message authentication tag.
       */
      string encryptCombined(
          const string& msg   ///< the input string that shall be encrypted
          );

      /** \brief Encrypts a string using the secret key the current nonce.
       *
       * \throws SodiumInvalidMessage if the input string is empty.
       *
       * \returns a pair of a string containing the encrypted data and an message authentication tag.
       */
      pair<string, SodiumLib::SecretBoxMac> encryptDetached(
          const string& msg   ///< the input string that shall be encrypted
          );

      /** \brief Decrypts a memory buffer using the secret key the current nonce.
       *
       * \throws SodiumInvalidCipher if the input buffer is empty or too short
       *
       * \returns a heap-allocated, protected buffer containing the decrypted data or an empty buffer
       * if the decryption failed (e.g., because the MAC verification failed).
       */
      SodiumSecureMemory decryptCombined(
          const MemView& cipher,   ///< the cipher text with the attached MAC
          SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked   ///< the protection class for the decrypted data buffer
          );

      /** \brief Decrypts a memory buffer using the secret key the current nonce.
       *
       * \throws SodiumInvalidCipher if the input buffer is empty or too short
       *
       * \throws SodiumInvalidMac if the MAC is empty
       *
       * \returns a heap-allocated, protected buffer containing the decrypted data or an empty buffer
       * if the decryption failed (e.g., because the MAC verification failed).
       */
      SodiumSecureMemory decryptDetached(
          const MemView& cipher,   ///< the pure cipher text
          const SodiumLib::SecretBoxMac& mac,   ///< the MAC for verifying the cipher text
          SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked   ///< the protection class for the decrypted data buffer
          );

      /** \brief Decrypts a string using the secret key the current nonce.
       *
       * \throws SodiumInvalidCipher if the input string is empty or too short
       *
       * \returns a string containing the decrypted data or an empty string
       * if the decryption failed (e.g., because the MAC verification failed).
       */
      string decryptCombined(
          const string& cipher   ///< the cipher text with the attached MAC
          );

      /** \brief Decrypts a string using the secret key the current nonce.
       *
       * \throws SodiumInvalidCipher if the input string is empty
       *
       * \throws SodiumInvalidMac if the MAC is empty
       *
       * \returns a string containing the decrypted data or an empty string
       * if the decryption failed (e.g., because the MAC verification failed).
       */
      string decryptDetached(
          const string& cipher,   ///< the pure cipher text
          const SodiumLib::SecretBoxMac& mac   ///< the MAC for verifying the cipher text
          );

      /** \brief Increments the internal nonce
       */
      void incrementNonce()
      {
        nonce.increment();
      }

      /** \returns a copy of the current nonce
       */
      SodiumLib::SecretBoxNonce currentNonce() const
      {
        return nonce.getNonce();
      }

      /** \returns a copy of the previous nonce
       */
      SodiumLib::SecretBoxNonce prevNonce() const
      {
        return nonce.getPrevNonce();
      }

    protected:
      /** \brief Internal convenience function for locking / unlocking of the secret key.
       *
       * \throws SodiumMemoryGuardException if the locking / unlocking failed
       */
      void setKeyLockState(
          bool setGuard   ///< if `true` the secret key will be locked (no access), if `false` the key will be set to read-only
          );

    private:
      SodiumLib* lib;
      SodiumLib::SecretBoxKey key;
      NonceBox<SodiumLib::SecretBoxNonce> nonce;
      bool autoIncrementNonce;
    };

    /** \brief A class that supports hashing multiple chunks of data
     */
    class GenericHasher
    {
    public:
      /** \brief Default ctor for a new hasher instance with a libsodium-internal standard key.
       *
       * The length of the produced hash is `crypto_generichash_BYTES` (currently 32 Byte / 256 bit).
       *
       * \throws SodiumNotAvailableException if the sodium wrapper singleton could be initialized / retrieved
       */
      GenericHasher();

      /** \brief Ctor for a new hasher instance with custom hash length and a libsodium-internal standard key.
       *
       * The `hashLen` parameter shall be between `crypto_generichash_BYTES_MIN` (16 bytes / 128 bit)
       * and `crypto_generichash_BYTES_MAX` (64 bytes / 512 bit).
       *
       * \throws SodiumNotAvailableException if the sodium wrapper singleton could be initialized / retrieved
       *
       * \throws std::range_error if the requested hash length is invalid
       */
      GenericHasher(
          size_t hashLen   ///< number of bytes for the resulting hash value
          );

      /** \brief Special ctor for a new hasher instance with a user provided key.
       *
       * The `hashLen` parameter shall be between `crypto_generichash_BYTES_MIN` (16 bytes / 128 bit)
       * and `crypto_generichash_BYTES_MAX` (64 bytes / 512 bit).
       *
       * \throws SodiumNotAvailableException if the sodium wrapper singleton could be initialized / retrieved
       *
       * \throws SodiumInvalidKey if the provided key is empty
       *
       * \throws std::range_error if the requested hash length is invalid
       */
      GenericHasher(
          const SodiumLib::GenericHashKey& k,
          size_t hashLen = crypto_generichash_BYTES
          );

      /** \brief Hashes another chunk of data
       *
       * If the hashing has already been finished by calling `finalize()`,
       * a call to this function simply does nothing.
       *
       * \throws SodiumInvalidMessage if the provided data is empty
       */
      void append(
          const MemView& inData   ///< the data to append to the hash calculation
          );

      /** \brief Hashes another chunk of data
       *
       * If the hashing has already been finished by calling `finalize()`,
       * a call to this function simply does nothing.
       *
       * \throws SodiumInvalidMessage if the provided data is empty
       */
      void append(
          const string& inData   ///< the data to append to the hash calculation
          );

      /** \brief Terminates the hash calculation and returns the hash value.
       *
       * \returns a heap-allocated memory buffer containing the hash or an
       * empty buffer if `finalize()` or `finalize_string()` has been called before.
       */
      MemArray finalize();

      /** \brief Terminates the hash calculation and returns the hash value.
       *
       * \returns a string containing the hash or an
       * empty string if `finalize()` or `finalize_string()` has been called before.
       */
      string finalize_string();

    private:
      SodiumLib* lib;
      size_t outLen{crypto_generichash_BYTES};
      crypto_generichash_state state;
      bool isFinalized{false};
    };

    /** \brief A class that facilitates the Diffie-Hellmann key exchange between
     * a client and a server.
     *
     * \note Internally, this class uses the older DH API of libsodium and
     * not the new `crypto_kx_*` functions.
     */
    class DiffieHellmannExchanger
    {
    public:
      /** \brief Ctor for a new instance; generates a new public / private key pair
       * for the key exchange.
       *
       * \throws SodiumNotAvailableException if the sodium wrapper singleton could be initialized / retrieved
       */
      explicit DiffieHellmannExchanger(
          bool _isClient   ///< set to `true` if we're on the client side or to `false` if we're on the server side
          );

      /** \returns the public key for sending it to the communication peer
       */
      SodiumLib::DH_PublicKey getMyPublicKey();

      /** \brief Computes the shared secret between client and server.
       *
       * \throws SodiumInvalidKey if the peer's public key is empty
       *
       * \returns the secret key shared between client and server
       */
      SodiumLib::DH_SharedSecret getSharedSecret(
          const SodiumLib::DH_PublicKey& othersPublicKey   ///< the public key of the communication partner
          );

    private:
      bool isClient;
      SodiumLib* lib;
      SodiumLib::DH_SecretKey sk;
      SodiumLib::DH_PublicKey pk;
    };

    /** \brief A class that facilitates the Diffie-Hellmann key exchange between
     * a client and a server.
     *
     * \note Internally, this class uses the new API of libsodium with the
     * `crypto_kx_*` functions. In particular, it creates a dedicated session key
     * for each direction of data flow (client -> server, server -> client).
     */
    class DiffieHellmannExchanger2
    {
    public:
      /** \brief Ctor for a new instance; generates a new, random public / private key pair
       * for the key exchange.
       *
       * \throws SodiumNotAvailableException if the sodium wrapper singleton could be initialized / retrieved
       */
      explicit DiffieHellmannExchanger2(
          bool _isClient   ///< set to `true` if we're on the client side or to `false` if we're on the server side
          );

      /** \brief Ctor for a new instance; generates a deterministic, seed-based public / private key pair
       * for the key exchange.
       *
       * \throws SodiumNotAvailableException if the sodium wrapper singleton could be initialized / retrieved
       *
       * \throws SodiumInvalidKey if the seed was invalid (wrong length or zero length)
       */
      DiffieHellmannExchanger2(
          bool _isClient,   ///< set to `true` if we're on the client side or to `false` if we're on the server side
          const string& seed_B64   ///< BASE64-encoded seed for the key generation
          );

      /** \returns the public key for sending it to the communication peer
       */
      SodiumLib::KX_PublicKey getMyPublicKey();

      /** \brief Computes the shared secret between client and server.
       *
       * \throws SodiumInvalidKey if the peer's public key is empty
       *
       * \returns a `<rx, tx>` pair of secret, symmetric session keys shared
       * between the peers
       */
      pair<SodiumLib::KX_SessionKey, SodiumLib::KX_SessionKey> getSessionKeys(
          const SodiumLib::KX_PublicKey& othersPublicKey   ///< the public key of the communication partner
          );

    private:
      bool isClient;
      SodiumLib* lib;
      SodiumLib::KX_SecretKey sk;
      SodiumLib::KX_PublicKey pk;
    };

    /** A class that encapsulates a secret (e.g., a symmetric key) in a password protected container.
     *
     * The password can be changed without changing the secret itself.
     */
    class PasswordProtectedSecret
    {
    public:
      // exceptions
      class PasswordHashingError{};
      class MalformedEncryptedData{};
      class NoPasswordSet{};
      class WrongPassword{};

      /** \brief Constructor for a new, empty secret that has no password set.
       */
      PasswordProtectedSecret(
          SodiumLib::PasswdHashStrength pwStrength = SodiumLib::PasswdHashStrength::Moderate,   ///< the hashing strength for the password
          SodiumLib::PasswdHashAlgo pwAlgo = SodiumLib::PasswdHashAlgo::Argon2id   ///< the algorithm to use for hashing the password
          );

      /** \brief Constructor for an existing, encrypted secret that has previously
       * been stored in our own, internal data format as returned by `asString()`.
       */
      PasswordProtectedSecret(
          const string& data,   ///< the encrypted data as returned by `asString()`
          bool isBase64=false   ///< set to true if the data is Base64 encoded
          );

      /** \brief Sets the secret to the provided data or erase the current secret
       * if the provided data buffer is empty.
       *
       * Before calling this function with non-empty data, a password has to be
       * set by calling `setPassword()`.
       *
       * \throws NoPasswordSet if there is no valid password available
       *
       * \returns `true` if the secret data has set / cleared successfully.
       */
      bool setSecret(const MemView& sec);

      /** \brief Sets the secret to the provided data or erase the current secret
       * if the provided data buffer is empty.
       *
       * Before calling this function with non-empty data, a password has to be
       * set by calling `setPassword()`.
       *
       * \throws NoPasswordSet if there is no valid password available
       *
       * \returns `true` if the secret data has set / cleared successfully.
       */
      bool setSecret(const string& sec);

      /** \brief Retrieves the currently stored secret if available.
       *
       * \throws NoPasswordSet if there is no valid password available
       *
       * \throws WrongPassword if the decryption of the stored secret failed; could also
       * indicate an integrity error of the stored secret.
       *
       * \returns the decrypted, stored secret or an empty buffer if there is currently no secret stored
       */
      string getSecretAsString();

      /** \brief Retrieves the currently stored secret if available.
       *
       * \throws NoPasswordSet if there is no valid password available
       *
       * \throws WrongPassword if the decryption of the stored secret failed; could also
       * indicate an integrity error of the stored secret.
       *
       * \returns the decrypted, stored secret or an empty buffer if there is currently no secret stored
       */
      SodiumSecureMemory getSecret(
          SodiumSecureMemType memType = SodiumSecureMemType::Locked   ///< the protection class for the decrypted secret
          );

      /** \brief Changes the password to a new value.
       *
       * Requires that a valid password has been set before using `setPassword()`.
       *
       * \throws NoPasswordSet if there is no existing, valid password
       *
       * \returns `false` if the old password was invalid or the new password is empty;
       * `true` if the password has been updated successfully and the stored secret has
       * successfully been re-encrypted using the new password.
       */
      bool changePassword(
          const string& oldPw,   ///< the old, existing password
          const string& newPw,   ///< the new password
          SodiumLib::PasswdHashStrength pwStrength = SodiumLib::PasswdHashStrength::Moderate,   ///< the hashing strength for the new password
          SodiumLib::PasswdHashAlgo pwAlgo = SodiumLib::PasswdHashAlgo::Argon2id   ///< the hashing algorithm for the new password
          );

      /** \brief Sets the initial password for encrypting / decrypting the stored secret.
       *
       * This function only succeeds if there has no password been set so far (read:
       * if there was no successfull call to `setPassword()` before).
       *
       * If we have been initialized with existing data, the provided
       * password is checked for validity. In this case this function only succeeds
       * if the provided password was correct.
       *
       * If we have been created "from scratch" without any data, any non-empty
       * password will do.
       *
       * \returns `false` if we already have a valid password, if the provided
       * password is empty or if the provided password was wrong; `true` if the
       * password has been set successfully.
       */
      bool setPassword(const string& pw);

      /** \returns `true` if we currently store (encrypted) secret content
       */
      bool hasContent() const { return cipher.notEmpty(); }

      /** \brief Checks whether a provided plain password matches the previously stored password
       * that has been set using `setPassword()`.
       *
       * \throws NoPasswordSet if there is no previously set password
       *
       * \returns `true` if the provided password matches the stored password set
       * via `setPassword()`; `false` otherwise
       */
      bool isValidPassword(
          const string& pw   ///< the plain, unencrypted password that shall be checked
          );

      /** \returns `true` if the user has previously set a valid password using 'setPassword()'
       */
      bool hasPassword() const { return (!(pwClear.empty())); }

      /** \brief Retrieves the complete data record including password hashing parameters.
       *
       * The data returned by this function should typically be stored on disk or
       * in a database for later re-loading.
       *
       * \returns a string that contains the password hashing parameters as well as the encrypted secret
       */
      string asString(
          bool useBase64 = false   ///< Base64-encode the resulting data
          ) const;

    private:
      SodiumLib* lib;
      SodiumLib::PwHashData hashConfig;
      MemArray cipher;
      SodiumLib::SecretBoxNonce nonce;
      SodiumSecureMemory pwClear;
      SodiumLib::SecretBoxKey symKey;

      void password2SymKey(const string& pw);
    };
  }
}
#endif
