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
#include <iostream>

#include <sodium.h>

//#include "../libSloppy.h"
#include "../Memory.h"

using namespace std;

namespace Sloppy
{
  namespace Net {
    class MessageBuilder;
  }

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
      SodiumSecureMemory()
        :rawPtr{nullptr}, nBytes{0}, type{SodiumSecureMemType::Normal},
          lib{nullptr}, curProtection{SodiumSecureMemAccess::NoAccess} {}

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

      // disable copy functions
      SodiumSecureMemory(const SodiumSecureMemory&) = delete; // no copy constructor
      SodiumSecureMemory& operator=(const SodiumSecureMemory&) = delete; // no copy assignment

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

      /** \brief Static function that creates a deep copy of an existing secure memory region
       *
       * I want copies of secure memory to be explicit. This is why I disabled the copy constructor
       * and the copy assignment operator and created explicit copy functions.
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
      static SodiumSecureMemory asCopy(const SodiumSecureMemory& src);

      /** \brief Creates a deep copy of the managed memory
       *
       * I want copies of secure memory to be explicit. This is why I disabled the copy constructor
       * and the copy assignment operator and created explicit copy functions.
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
      SodiumSecureMemory copy() const;

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

    private:
      void* rawPtr;
      size_t nBytes;
      SodiumSecureMemType type;
      SodiumLib* lib;
      SodiumSecureMemAccess curProtection;

    };

    //----------------------------------------------------------------------------

    /** \brief An enum class for distinguishing between secret (private) and public cryptographic keys
     */
    enum class SodiumKeyType
    {
      Secret,   ///< the key is a secret key (either a private, asymetric key or a symetric key)
      Public    ///< the key is a public asymetric key
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
       * The memory for secret keys is **not** protected after initialization. It can be directly
       * accessed for reading and writing.
       */
      SodiumKey()
        : SodiumSecureMemory{keySize, (kt == SodiumKeyType::Secret) ? SodiumSecureMemType::Guarded : SodiumSecureMemType::Normal},
          keyType{kt} {}

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

      /** \brief Creates a deep copy of a key.
       *
       * Calls internally the ctor for SodiumKey and can thus throw all exceptions
       * that are also thrown by the ctor.
       *
       * \throws SodiumKeyLocked if a secret shall be copied that is currently not read-accessible
       */
      static SodiumKey asCopy(
            const SodiumKey& src   ///< the key to copy
            )
      {
        if (!(src.canRead()))
        {
          throw SodiumKeyLocked{"creating a key copy"};
        }

        SodiumKey cpy;
        SodiumSecureMemAccess srcProtection = src.getProtection();
        if (kt == SodiumKeyType::Secret)
        {
          cpy.setAccess(SodiumSecureMemAccess::RW);
        }

        memcpy(cpy.toNotOwningArray().to_voidPtr(), src.toNotOwningArray().to_voidPtr(), src.size());

        if (kt == SodiumKeyType::Secret)
        {
          cpy.setAccess(srcProtection);
        }

        return cpy;
      }

      /** \brief Creates a deep copy of the key.
       *
       * Calls internally the ctor for SodiumKey and can thus throw all exceptions
       * that are also thrown by the ctor.
       *
       * \throws SodiumKeyLocked if a secret shall be copied that is currently not read-accessible
       */
      SodiumKey copy() const
      {
        return asCopy(*this);
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
        memcpy(rawPtr, data.to_voidPtr(), keySize);
        return true;
      }

    protected:
      SodiumKeyType keyType;
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
      int (*crypto_aead_chacha20poly1305_encrypt)(unsigned char *c, unsigned long long *clen,
                                                  const unsigned char *m, unsigned long long mlen,
                                                  const unsigned char *ad, unsigned long long adlen,
                                                  const unsigned char *nsec, const unsigned char *npub,
                                                  const unsigned char *k);
      int (*crypto_aead_chacha20poly1305_decrypt)(unsigned char *m, unsigned long long *mlen,
                                                  unsigned char *nsec,
                                                  const unsigned char *c, unsigned long long clen,
                                                  const unsigned char *ad, unsigned long long adlen,
                                                  const unsigned char *npub, const unsigned char *k);
      int (*crypto_aead_chacha20poly1305_encrypt_detached)(unsigned char *c, unsigned char *mac,
                                                           unsigned long long *maclen_p,
                                                           const unsigned char *m, unsigned long long mlen,
                                                           const unsigned char *ad, unsigned long long adlen,
                                                           const unsigned char *nsec, const unsigned char *npub,
                                                           const unsigned char *k);
      int (*crypto_aead_chacha20poly1305_decrypt_detached)(unsigned char *m, unsigned char *nsec,
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
      using SecretBoxKeyType = SodiumKey<SodiumKeyType::Secret, crypto_secretbox_KEYBYTES>;
      using SecretBoxNonceType = SodiumKey<SodiumKeyType::Public, crypto_secretbox_NONCEBYTES>;
      using SecretBoxMacType = SodiumKey<SodiumKeyType::Public, crypto_secretbox_MACBYTES>;

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
      MemArray crypto_secretbox_easy(
          const MemView& msg,   ///< the plain message for encryption
          const SecretBoxNonceType& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKeyType& key        ///< the secret key; the caller has to ensure that the memory is enabled for reading
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
      SodiumSecureMemory crypto_secretbox_open_easy__secure(
          const MemView& cipher,   ///< the buffer with the encrypted message and the attached MAC
          const SecretBoxNonceType& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKeyType& key,        ///< the secret key; the caller has to ensure that the memory is enabled for reading
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
      MemArray crypto_secretbox_open_easy(
          const MemView& cipher,   ///< the buffer with the encrypted message and the attached MAC
          const SecretBoxNonceType& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKeyType& key        ///< the secret key; the caller has to ensure that the memory is enabled for reading
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
      pair<MemArray, SecretBoxMacType> crypto_secretbox_detached(
          const MemView& msg,   ///< the plain message for encryption
          const SecretBoxNonceType& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKeyType& key        ///< the secret key; the caller has to ensure that the memory is enabled for reading
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
      SodiumSecureMemory crypto_secretbox_open_detached(
          const MemView& cipher,   ///< a buffer with the pure, encrypted message without MAC attachment
          const SecretBoxMacType& mac,   ///< the MAC for the message
          const SecretBoxNonceType& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKeyType& key,        ///< the secret key; the caller has to ensure that the memory is enabled for reading
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
      string crypto_secretbox_easy(
          const string& msg,   ///< a string with the plain text message for encryption
          const SecretBoxNonceType& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKeyType& key        ///< the secret key; the caller has to ensure that the memory is enabled for reading
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
      string crypto_secretbox_open_easy(
          const string& cipher,   ///< a string with the encrypted message and the attached MAC
          const SecretBoxNonceType& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKeyType& key        ///< the secret key; the caller has to ensure that the memory is enabled for reading
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
      pair<string, SecretBoxMacType> crypto_secretbox_detached(
          const string& msg,   ///< the plain message for encryption
          const SecretBoxNonceType& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKeyType& key        ///< the secret key; the caller has to ensure that the memory is enabled for reading
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
      string crypto_secretbox_open_detached(
          const string& cipher,   ///< a string with the pure, encrypted message without MAC attachment
          const SecretBoxMacType& mac,   ///< the MAC for the message
          const SecretBoxNonceType& nonce,   ///< the nonce to be used for this specific message
          const SecretBoxKeyType& key        ///< the secret key; the caller has to ensure that the memory is enabled for reading
          );

      // message authentication
      using AuthKeyType = SodiumKey<SodiumKeyType::Secret, crypto_auth_KEYBYTES>;
      using AuthTagType = SodiumKey<SodiumKeyType::Public, crypto_auth_BYTES>;
      AuthTagType crypto_auth(const MemView& msg, const AuthKeyType& key);
      bool crypto_auth_verify(const MemView& msg, const AuthTagType& tag, const AuthKeyType& key);
      string crypto_auth(const string& msg, const string& key);
      bool crypto_auth_verify(const string& msg, const string& tag, const string& key);

      // authenticated encryption with additional data, buffer based
      using AEAD_ChaCha20Poly1305_KeyType = SodiumKey<SodiumKeyType::Secret, crypto_aead_chacha20poly1305_KEYBYTES>;
      using AEAD_ChaCha20Poly1305_NonceType = SodiumKey<SodiumKeyType::Public, crypto_aead_chacha20poly1305_NPUBBYTES>;
      using AEAD_ChaCha20Poly1305_TagType = SodiumKey<SodiumKeyType::Public, crypto_aead_chacha20poly1305_ABYTES>;
      ManagedBuffer crypto_aead_chacha20poly1305_encrypt(const MemView& msg, const AEAD_ChaCha20Poly1305_NonceType& nonce,
                                                         const AEAD_ChaCha20Poly1305_KeyType& key, const ManagedBuffer& ad = ManagedBuffer{});
      SodiumSecureMemory crypto_aead_chacha20poly1305_decrypt(const MemView& cipher, const AEAD_ChaCha20Poly1305_NonceType& nonce,
                                                              const AEAD_ChaCha20Poly1305_KeyType& key, const ManagedBuffer& ad = ManagedBuffer{},
                                                              SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked);

      // authenticated encryption with additional data, string-based
      string crypto_aead_chacha20poly1305_encrypt(const string& msg, const string& nonce, const string& key, const string& ad = string{});
      string crypto_aead_chacha20poly1305_decrypt(const string& cipher, const string& nonce, const string& key, const string& ad = string{});

      // authenticated encryption with additional data, mixed form
      string crypto_aead_chacha20poly1305_encrypt(const string& msg, const AEAD_ChaCha20Poly1305_NonceType& nonce, const AEAD_ChaCha20Poly1305_KeyType& key,
                                                  const string& ad = string{});
      string crypto_aead_chacha20poly1305_decrypt(const string& cipher, const AEAD_ChaCha20Poly1305_NonceType& nonce, const AEAD_ChaCha20Poly1305_KeyType& key,
                                                  const string& ad = string{});

      // authenticated encryption with additional data, AES-256 GCM, buffer based
      using AEAD_AES256GCM_KeyType = SodiumKey<SodiumKeyType::Secret, crypto_aead_aes256gcm_KEYBYTES>;
      using AEAD_AES256GCM_NonceType = SodiumKey<SodiumKeyType::Public, crypto_aead_aes256gcm_NPUBBYTES>;
      using AEAD_AES256GCM_TagType = SodiumKey<SodiumKeyType::Public, crypto_aead_aes256gcm_ABYTES>;
      bool is_AES256GCM_avail();
      ManagedBuffer crypto_aead_aes256gcm_encrypt(const MemView& msg, const AEAD_AES256GCM_NonceType& nonce,
                                                         const AEAD_AES256GCM_KeyType& key, const ManagedBuffer& ad = ManagedBuffer{});
      SodiumSecureMemory crypto_aead_aes256gcm_decrypt(const MemView& cipher, const AEAD_AES256GCM_NonceType& nonce,
                                                              const AEAD_AES256GCM_KeyType& key, const ManagedBuffer& ad = ManagedBuffer{},
                                                              SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked);

      // authenticated encryption with additional data, AES-256 GCM, string-based
      string crypto_aead_aes256gcm_encrypt(const string& msg, const string& nonce, const string& key, const string& ad = string{});
      string crypto_aead_aes256gcm_decrypt(const string& cipher, const string& nonce, const string& key, const string& ad = string{});

      // authenticated encryption with additional data, AES-256 GCM, mixed form
      string crypto_aead_aes256gcm_encrypt(const string& msg, const AEAD_AES256GCM_NonceType& nonce, const AEAD_AES256GCM_KeyType& key,
                                                  const string& ad = string{});
      string crypto_aead_aes256gcm_decrypt(const string& cipher, const AEAD_AES256GCM_NonceType& nonce, const AEAD_AES256GCM_KeyType& key,
                                                  const string& ad = string{});

      // public key cryptography, key handling
      using AsymCrypto_PublicKey = SodiumKey<SodiumKeyType::Public, crypto_box_PUBLICKEYBYTES>;
      using AsymCrypto_SecretKey = SodiumKey<SodiumKeyType::Secret, crypto_box_SECRETKEYBYTES>;
      using AsymCrypto_KeySeed = SodiumKey<SodiumKeyType::Secret, crypto_box_SEEDBYTES>;
      bool genAsymCryptoKeyPair(AsymCrypto_PublicKey& pk_out, AsymCrypto_SecretKey& sk_out);
      bool genAsymCryptoKeyPairSeeded(const AsymCrypto_KeySeed& seed, AsymCrypto_PublicKey& pk_out, AsymCrypto_SecretKey& sk_out);
      bool genPublicCryptoKeyFromSecretKey(const AsymCrypto_SecretKey& sk, AsymCrypto_PublicKey& pk_out);

      // public key cryptography, encryption / decryption, buffer-based
      using AsymCrypto_Nonce = SodiumKey<SodiumKeyType::Public, crypto_box_NONCEBYTES>;
      using AsymCrypto_Tag = SodiumKey<SodiumKeyType::Public, crypto_box_MACBYTES>;
      ManagedBuffer crypto_box_easy(const MemView& msg, const AsymCrypto_Nonce& nonce,
                                    const AsymCrypto_PublicKey& recipientKey, const AsymCrypto_SecretKey& senderKey);
      SodiumSecureMemory crypto_box_open_easy(const MemView& cipher, const AsymCrypto_Nonce& nonce, const AsymCrypto_PublicKey& senderKey,
                                              const AsymCrypto_SecretKey& recipientKey, SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked);
      pair<ManagedBuffer, AsymCrypto_Tag> crypto_box_detached(const MemView& msg, const AsymCrypto_Nonce& nonce,
                                    const AsymCrypto_PublicKey& recipientKey, const AsymCrypto_SecretKey& senderKey);
      SodiumSecureMemory crypto_box_open_detached(const MemView& cipher, const AsymCrypto_Tag& mac, const AsymCrypto_Nonce& nonce, const AsymCrypto_PublicKey& senderKey,
                                                  const AsymCrypto_SecretKey& recipientKey, SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked);

      // public key cryptography, encryption / decryption, string-based
      // Note: I'll not provide encryption / decryption based on string-keys because
      // there is point in doing so. Keys have to be specially generated and thus
      // can't be simple strings
      string crypto_box_easy(const string& msg, const AsymCrypto_Nonce& nonce,
                                    const AsymCrypto_PublicKey& recipientKey, const AsymCrypto_SecretKey& senderKey);
      string crypto_box_open_easy(const string& cipher, const AsymCrypto_Nonce& nonce, const AsymCrypto_PublicKey& senderKey, const AsymCrypto_SecretKey& recipientKey);
      pair<string, string> crypto_box_detached(const string& msg, const AsymCrypto_Nonce& nonce,
                                    const AsymCrypto_PublicKey& recipientKey, const AsymCrypto_SecretKey& senderKey);
      string crypto_box_open_detached(const string& cipher, const string& mac, const AsymCrypto_Nonce& nonce, const AsymCrypto_PublicKey& senderKey,
                                      const AsymCrypto_SecretKey& recipientKey);

      // signatures, key handling
      using AsymSign_PublicKey = SodiumKey<SodiumKeyType::Public, crypto_sign_PUBLICKEYBYTES>;
      using AsymSign_SecretKey = SodiumKey<SodiumKeyType::Secret, crypto_sign_SECRETKEYBYTES>;
      using AsymSign_KeySeed = SodiumKey<SodiumKeyType::Public, crypto_sign_SEEDBYTES>;
      void genAsymSignKeyPair(AsymSign_PublicKey& pk_out, AsymSign_SecretKey& sk_out);
      bool genAsymSignKeyPairSeeded(const AsymSign_KeySeed& seed, AsymSign_PublicKey& pk_out, AsymSign_SecretKey& sk_out);
      bool genPublicSignKeyFromSecretKey(const AsymSign_SecretKey& sk, AsymSign_PublicKey& pk_out);
      bool genSignKeySeedFromSecretKey(const AsymSign_SecretKey& sk, AsymSign_KeySeed& seed_out);

      // signatures, buffer-based
      using AsymSign_Signature = SodiumKey<SodiumKeyType::Public, crypto_sign_BYTES>;
      ManagedBuffer crypto_sign(const MemView& msg, const AsymSign_SecretKey& sk);
      ManagedBuffer crypto_sign_open(const MemView& signedMsg, const AsymSign_PublicKey& pk);
      bool crypto_sign_detached(const MemView& msg, const AsymSign_SecretKey& sk, AsymSign_Signature& sig_out);
      bool crypto_sign_verify_detached(const MemView& msg, const AsymSign_Signature& sig, const AsymSign_PublicKey& pk);

      // signatures, string-based
      string crypto_sign(const string& msg, const AsymSign_SecretKey& sk);
      string crypto_sign_open(const string& signedMsg, const AsymSign_PublicKey& pk);
      string crypto_sign_detached(const string& msg, const AsymSign_SecretKey& sk);
      bool crypto_sign_verify_detached(const string& msg, const string& sig, const AsymSign_PublicKey& pk);

      // hashing
      using GenericHashKey = SodiumKey<SodiumKeyType::Secret, crypto_generichash_KEYBYTES>;
      using ShorthashKey = SodiumKey<SodiumKeyType::Secret, crypto_shorthash_KEYBYTES>;
      ManagedBuffer crypto_generichash(const MemView& inData);
      ManagedBuffer crypto_generichash(const MemView& inData, const GenericHashKey& key);
      string crypto_generichash(const string& inData);
      string crypto_generichash(const string& inData, const GenericHashKey& key);
      bool crypto_generichash_init(crypto_generichash_state *state);
      bool crypto_generichash_init(crypto_generichash_state *state, const GenericHashKey& k);
      bool crypto_generichash_update(crypto_generichash_state *state, const MemView& inData);
      bool crypto_generichash_update(crypto_generichash_state *state, const string& inData);
      ManagedBuffer crypto_generichash_final(crypto_generichash_state *state);
      string crypto_generichash_final_string(crypto_generichash_state *state);
      ManagedBuffer crypto_shorthash(const MemView& inData, const ShorthashKey& k);
      string crypto_shorthash(const string& inData, const ShorthashKey& k);

      // password hashing and key derivation
      enum class PasswdHashStrength
      {
        Interactive,
        Moderate,
        High
      };
      enum class PasswdHashAlgo
      {
        Argon2,
        Scrypt
      };
      struct PwHashData
      {
        ManagedBuffer salt;
        unsigned long long opslimit;
        size_t memlimit;
        PasswdHashAlgo algo;
      };

      pair<SodiumSecureMemory, PwHashData> crypto_pwhash(const MemView& pw, size_t hashLen,
                                                            PasswdHashStrength strength = PasswdHashStrength::Moderate,
                                                            PasswdHashAlgo algo = PasswdHashAlgo::Argon2,
                                                            SodiumSecureMemType memType = SodiumSecureMemType::Locked);
      SodiumSecureMemory crypto_pwhash(const MemView& pw, size_t hashLen, PwHashData& hDat,
                                       SodiumSecureMemType memType = SodiumSecureMemType::Locked);
      pair<string, string> crypto_pwhash(const string& pw, size_t hashLen,
                                         PasswdHashStrength strength = PasswdHashStrength::Moderate,
                                         PasswdHashAlgo algo = PasswdHashAlgo::Argon2,
                                         SodiumSecureMemType memType = SodiumSecureMemType::Locked);
      string crypto_pwhash_str(const MemView& pw, PasswdHashStrength strength = PasswdHashStrength::Moderate,
                               PasswdHashAlgo algo = PasswdHashAlgo::Argon2);
      string crypto_pwhash_str(const string& pw, PasswdHashStrength strength = PasswdHashStrength::Moderate,
                               PasswdHashAlgo algo = PasswdHashAlgo::Argon2);
      bool crypto_pwhash_str_verify(const MemView& pw, const string& hashResult, PasswdHashAlgo algo = PasswdHashAlgo::Argon2);
      bool crypto_pwhash_str_verify(const string& pw, const string& hashResult, PasswdHashAlgo algo = PasswdHashAlgo::Argon2);

      // converts password hash strength and algorithm into values for opslimit and memlimit
      pair<unsigned long long, size_t> pwHashConfigToValues(PasswdHashStrength strength, PasswdHashAlgo algo);

      // Diffie-Hellmann key exchange
      using DH_PublicKey = SodiumKey<SodiumKeyType::Public, crypto_scalarmult_BYTES>;
      using DH_SecretKey = SodiumKey<SodiumKeyType::Secret, crypto_scalarmult_SCALARBYTES>;
      using DH_SharedSecret = SodiumKey<SodiumKeyType::Secret, crypto_scalarmult_BYTES>;
      bool genDHKeyPair(DH_PublicKey& pk_out, DH_SecretKey& sk_out);
      bool genDHSharedSecret(const DH_SecretKey& mySecretKey, const DH_PublicKey& othersPublicKey, DH_SharedSecret& sh_out);
      bool genPublicDHKeyFromSecretKey(const DH_SecretKey& sk, DH_PublicKey& pk_out);


    protected:
      SodiumLib(void* _libHandle);
      static unique_ptr<SodiumLib> inst;
      void *libHandle;
      SodiumPtr sodium;

      bool crypto_secretbox_open_easy__internal(const MemArray& targetBuf, const MemView& cipher, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key);

      // generic handler for AEAD encryption, buffer-based
      ManagedBuffer crypto_aead_encrypt(int (*funcPtr)(unsigned char *, unsigned long long *, const unsigned char *,
                                                       unsigned long long, const unsigned char *, unsigned long long,
                                                       const unsigned char *, const unsigned char *,const unsigned char *),
                                        size_t tagSize, const MemView& msg, const MemView& nonce,
                                        const MemView& key, const ManagedBuffer& ad = ManagedBuffer{});

      // generic handler for AEAD decryption, buffer-based
      SodiumSecureMemory crypto_aead_decrypt(int (*funcPtr)(unsigned char *, unsigned long long *, unsigned char *,
                                                            const unsigned char *, unsigned long long, const unsigned char *,
                                                            unsigned long long, const unsigned char *, const unsigned char *),
                                             size_t tagSize, const MemView& cipher, const MemView& nonce,
                                                              const MemView& key, const ManagedBuffer& ad = ManagedBuffer{},
                                                              SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked);
      // generic handler for AEAD encryption, string-based
      string crypto_aead_encrypt(int (*funcPtr)(unsigned char *, unsigned long long *, const unsigned char *,
                                                       unsigned long long, const unsigned char *, unsigned long long,
                                                       const unsigned char *, const unsigned char *,const unsigned char *),
                                        size_t nonceSize, size_t keySize, size_t tagSize, const string& msg, const string& nonce,
                                        const string& key, const string& ad = string{});

      // generic handler for AEAD decryption, string-based
      string crypto_aead_decrypt(int (*funcPtr)(unsigned char *, unsigned long long *, unsigned char *,
                                                            const unsigned char *, unsigned long long, const unsigned char *,
                                                            unsigned long long, const unsigned char *, const unsigned char *),
                                             size_t nonceSize, size_t keySize, size_t tagSize, const string& cipher,
                                             const string& nonce, const string& key, const string& ad = string{});

      // generic handler for AEAD encryption, mixed form
      string crypto_aead_encrypt(int (*funcPtr)(unsigned char *, unsigned long long *, const unsigned char *,
                                                       unsigned long long, const unsigned char *, unsigned long long,
                                                       const unsigned char *, const unsigned char *,const unsigned char *),
                                        size_t tagSize, const string& msg, const MemView& nonce,
                                        const MemView& key, const string& ad = string{});

      // generic handler for AEAD decryption, mixed form
      string crypto_aead_decrypt(int (*funcPtr)(unsigned char *, unsigned long long *, unsigned char *,
                                                            const unsigned char *, unsigned long long, const unsigned char *,
                                                            unsigned long long, const unsigned char *, const unsigned char *),
                                             size_t tagSize, const string& cipher, const MemView& nonce,
                                                              const MemView& key, const string& ad = string{});
    };

    // a template class that provides nonce handling for "crypto boxes"
    template <typename NonceType>
    class NonceBox
    {
    public:
      NonceBox(const NonceType& _nonce, bool autoIncNonce = false)
      :nonceIncrementCount{0}, autoIncrementNonce{autoIncNonce}, lib{SodiumLib::getInstance()}
      {
        if (lib == nullptr)
        {
          throw SodiumNotAvailableException{};
        }

        initialNonce = _nonce.copy();
        nextNonce = _nonce.copy();
        lastNonce = _nonce.copy();
      }

      void incrementNonce()
      {
        if (nonceIncrementCount > 0) lib->increment(lastNonce);

        lib->increment(nextNonce);

        ++nonceIncrementCount;
      }

      NonceType getLastNonce() const { return NonceType::asCopy(lastNonce); }

      size_t getNonceIncrementCount() const { return nonceIncrementCount; }

      void resetNonce()
      {
        nextNonce = initialNonce.copy();
        lastNonce = initialNonce.copy();
        nonceIncrementCount = 0;
      }

      void setNonce(const NonceType& n)
      {
        initialNonce = n.copy();
        resetNonce();
      }

    protected:
      NonceType initialNonce;
      NonceType lastNonce;
      NonceType nextNonce;
      size_t nonceIncrementCount;
      bool autoIncrementNonce;
      SodiumLib* lib;
    };

    // a wrapper class for easy symmetric encryption / decryption
    class SodiumSecretBox : public NonceBox<SodiumKey<SodiumKeyType::Public, crypto_secretbox_NONCEBYTES>>
    {
    public:
      using KeyType = SodiumKey<SodiumKeyType::Secret, crypto_secretbox_KEYBYTES>;
      using NonceType = SodiumKey<SodiumKeyType::Public, crypto_secretbox_NONCEBYTES>;

      SodiumSecretBox(const KeyType& _key, const NonceType& _nonce, bool autoIncNonce = false);

      // encryption
      ManagedBuffer encryptCombined(const MemView& msg);
      pair<ManagedBuffer, ManagedBuffer> encryptDetached(const MemView& msg);
      string encryptCombined(const string& msg);
      pair<string, string> encryptDetached(const string& msg);

      // decryption
      SodiumSecureMemory decryptCombined(const MemView& cipher, SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked);
      SodiumSecureMemory decryptDetached(const MemView& cipher, const MemView& mac, SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked);
      string decryptCombined(const string& cipher);
      string decryptDetached(const string& cipher, const string& mac);

    protected:
      void setKeyLockState(bool setGuard);

    private:
      KeyType key;
    };

    // a class that calculates a hash over multiple chunks of data
    class GenericHasher
    {
    public:
      GenericHasher();
      GenericHasher(const SodiumLib::GenericHashKey& k);

      bool append(const MemView& inData);
      bool append(const string& inData);

      ManagedBuffer finalize();
      string finalize_string();

    private:
      crypto_generichash_state state;
      bool isFinalized;
      SodiumLib* lib;

    };

    // a helper class for Diffie-Hellmann key exchange
    // including additional hashing of the calculated
    // shared secret
    class DiffieHellmannExchanger
    {
    public:
      using SharedSecret = SodiumKey<SodiumKeyType::Secret, crypto_generichash_BYTES>;

      DiffieHellmannExchanger(bool _isClient);
      SodiumLib::DH_PublicKey getMyPublicKey();
      SharedSecret getSharedSecret(const SodiumLib::DH_PublicKey& othersPublicKey);

    private:
      bool isClient;
      SodiumLib* lib;
      SodiumLib::DH_SecretKey sk;
      SodiumLib::DH_PublicKey pk;
    };

    // a class that encapsulates a secret (e.g., a symmetric key)
    // in a password protected container.
    //
    // the password can be changed without changing the
    // secret itself.
    class PasswordProtectedSecret
    {
    public:
      // exceptions
      class PasswordHashingError{};
      class MalformedEncryptedData{};
      class NoPasswordSet{};
      class WrongPassword{};

      // constructor for a new, empty secret
      PasswordProtectedSecret(SodiumLib::PasswdHashStrength pwStrength = SodiumLib::PasswdHashStrength::Moderate,
                              SodiumLib::PasswdHashAlgo pwAlgo = SodiumLib::PasswdHashAlgo::Argon2);

      // constructor for an existing, encrypted secret
      PasswordProtectedSecret(const string& data, bool isBase64=false);

      // access functions
      bool setSecret(const MemView& sec);
      bool setSecret(const string& sec);
      string getSecretAsString();
      SodiumSecureMemory getSecret(SodiumSecureMemType memType = SodiumSecureMemType::Locked);

      // password handling
      bool changePassword(const string& oldPw, const string& newPw,
                          SodiumLib::PasswdHashStrength pwStrength = SodiumLib::PasswdHashStrength::Moderate,
                          SodiumLib::PasswdHashAlgo pwAlgo = SodiumLib::PasswdHashAlgo::Argon2);
      bool setPassword(const string& pw);

      // boolean queries
      bool hasContent() const { return cipher.isValid(); }
      bool isValidPassword(const string& pw);
      bool hasPassword() const { return pwClear.isValid(); }

      // getters for the encrypted data including hashing parameters
      string asString(bool useBase64 = false) const;

    protected:
      Net::MessageBuilder hashConfigToBin() const;

    private:
      SodiumLib* lib;
      SodiumLib::PwHashData hashConfig;
      ManagedBuffer cipher;
      SodiumLib::SecretBoxNonceType nonce;
      SodiumSecureMemory pwClear;
      SodiumLib::SecretBoxKeyType symKey;

      void password2SymKey(const string& pw);
    };
  }
}
#endif
