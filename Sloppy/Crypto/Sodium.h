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

      // size change
      virtual void shrink(size_t newSize) override;

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

      SodiumKey<kt, keySize> copy() const
      {
        return asCopy(*this);
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
      void increment(const ManagedMemory& buf);
      void add(const ManagedMemory& a, const ManagedMemory& b);

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

      // message authentication
      using AuthKeyType = SodiumKey<SodiumKeyType::Secret, crypto_auth_KEYBYTES>;
      using AuthTagType = SodiumKey<SodiumKeyType::Public, crypto_auth_BYTES>;
      AuthTagType crypto_auth(const ManagedMemory& msg, const AuthKeyType& key);
      bool crypto_auth_verify(const ManagedMemory& msg, const AuthTagType& tag, const AuthKeyType& key);
      string crypto_auth(const string& msg, const string& key);
      bool crypto_auth_verify(const string& msg, const string& tag, const string& key);

      // authenticated encryption with additional data, buffer based
      using AEAD_ChaCha20Poly1305_KeyType = SodiumKey<SodiumKeyType::Secret, crypto_aead_chacha20poly1305_KEYBYTES>;
      using AEAD_ChaCha20Poly1305_NonceType = SodiumKey<SodiumKeyType::Public, crypto_aead_chacha20poly1305_NPUBBYTES>;
      using AEAD_ChaCha20Poly1305_TagType = SodiumKey<SodiumKeyType::Public, crypto_aead_chacha20poly1305_ABYTES>;
      ManagedBuffer crypto_aead_chacha20poly1305_encrypt(const ManagedMemory& msg, const AEAD_ChaCha20Poly1305_NonceType& nonce,
                                                         const AEAD_ChaCha20Poly1305_KeyType& key, const ManagedBuffer& ad = ManagedBuffer{});
      SodiumSecureMemory crypto_aead_chacha20poly1305_decrypt(const ManagedMemory& cipher, const AEAD_ChaCha20Poly1305_NonceType& nonce,
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
      ManagedBuffer crypto_aead_aes256gcm_encrypt(const ManagedMemory& msg, const AEAD_AES256GCM_NonceType& nonce,
                                                         const AEAD_AES256GCM_KeyType& key, const ManagedBuffer& ad = ManagedBuffer{});
      SodiumSecureMemory crypto_aead_aes256gcm_decrypt(const ManagedMemory& cipher, const AEAD_AES256GCM_NonceType& nonce,
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
      void genAsymCryptoKeyPair(AsymCrypto_PublicKey& pk_out, AsymCrypto_SecretKey& sk_out);
      bool genAsymCryptoKeyPairSeeded(const AsymCrypto_KeySeed& seed, AsymCrypto_PublicKey& pk_out, AsymCrypto_SecretKey& sk_out);
      bool genPublicCryptoKeyFromSecretKey(const AsymCrypto_SecretKey& sk, AsymCrypto_PublicKey& pk_out);

      // public key cryptography, encryption / decryption, buffer-based
      using AsymCrypto_Nonce = SodiumKey<SodiumKeyType::Public, crypto_box_NONCEBYTES>;
      using AsymCrypto_Tag = SodiumKey<SodiumKeyType::Public, crypto_box_MACBYTES>;
      ManagedBuffer crypto_box_easy(const ManagedMemory& msg, const AsymCrypto_Nonce& nonce,
                                    const AsymCrypto_PublicKey& recipientKey, const AsymCrypto_SecretKey& senderKey);
      SodiumSecureMemory crypto_box_open_easy(const ManagedMemory& cipher, const AsymCrypto_Nonce& nonce, const AsymCrypto_PublicKey& senderKey,
                                              const AsymCrypto_SecretKey& recipientKey, SodiumSecureMemType clearTextProtection = SodiumSecureMemType::Locked);
      pair<ManagedBuffer, AsymCrypto_Tag> crypto_box_detached(const ManagedMemory& msg, const AsymCrypto_Nonce& nonce,
                                    const AsymCrypto_PublicKey& recipientKey, const AsymCrypto_SecretKey& senderKey);
      SodiumSecureMemory crypto_box_open_detached(const ManagedMemory& cipher, const AsymCrypto_Tag& mac, const AsymCrypto_Nonce& nonce, const AsymCrypto_PublicKey& senderKey,
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
      ManagedBuffer crypto_sign(const ManagedMemory& msg, const AsymSign_SecretKey& sk);
      ManagedBuffer crypto_sign_open(const ManagedMemory& signedMsg, const AsymSign_PublicKey& pk);
      bool crypto_sign_detached(const ManagedMemory& msg, const AsymSign_SecretKey& sk, AsymSign_Signature& sig_out);
      bool crypto_sign_verify_detached(const ManagedMemory& msg, const AsymSign_Signature& sig, const AsymSign_PublicKey& pk);

      // signatures, string-based
      string crypto_sign(const string& msg, const AsymSign_SecretKey& sk);
      string crypto_sign_open(const string& signedMsg, const AsymSign_PublicKey& pk);
      string crypto_sign_detached(const string& msg, const AsymSign_SecretKey& sk);
      bool crypto_sign_verify_detached(const string& msg, const string& sig, const AsymSign_PublicKey& pk);


    protected:
      SodiumLib(void* _libHandle);
      static unique_ptr<SodiumLib> inst;
      void *libHandle;
      SodiumPtr sodium;

      // generic handler for AEAD encryption, buffer-based
      ManagedBuffer crypto_aead_encrypt(int (*funcPtr)(unsigned char *, unsigned long long *, const unsigned char *,
                                                       unsigned long long, const unsigned char *, unsigned long long,
                                                       const unsigned char *, const unsigned char *,const unsigned char *),
                                        size_t tagSize, const ManagedMemory& msg, const ManagedMemory& nonce,
                                        const ManagedMemory& key, const ManagedBuffer& ad = ManagedBuffer{});

      // generic handler for AEAD decryption, buffer-based
      SodiumSecureMemory crypto_aead_decrypt(int (*funcPtr)(unsigned char *, unsigned long long *, unsigned char *,
                                                            const unsigned char *, unsigned long long, const unsigned char *,
                                                            unsigned long long, const unsigned char *, const unsigned char *),
                                             size_t tagSize, const ManagedMemory& cipher, const ManagedMemory& nonce,
                                                              const ManagedMemory& key, const ManagedBuffer& ad = ManagedBuffer{},
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
                                        size_t tagSize, const string& msg, const ManagedMemory& nonce,
                                        const ManagedMemory& key, const string& ad = string{});

      // generic handler for AEAD decryption, mixed form
      string crypto_aead_decrypt(int (*funcPtr)(unsigned char *, unsigned long long *, unsigned char *,
                                                            const unsigned char *, unsigned long long, const unsigned char *,
                                                            unsigned long long, const unsigned char *, const unsigned char *),
                                             size_t tagSize, const string& cipher, const ManagedMemory& nonce,
                                                              const ManagedMemory& key, const string& ad = string{});
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
    };

  }
}
#endif
