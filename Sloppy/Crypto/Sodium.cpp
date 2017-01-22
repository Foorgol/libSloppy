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

#include <dlfcn.h>
#include <iostream>
#include <malloc.h>
#include <cstring>

#include <sodium.h>

#include "Sodium.h"

namespace Sloppy
{
  namespace Crypto
  {
    // allocate memory for the static instance pointer
    unique_ptr<SodiumLib> SodiumLib::inst;

    //----------------------------------------------------------------------------

    SodiumSecureMemory::SodiumSecureMemory(size_t _len, SodiumSecureMemType t)
      :ManagedMemory{_len}, type{t}, lib{SodiumLib::getInstance()},
        curProtection{SodiumSecureMemAccess::RW}
    {
      if (lib == nullptr)
      {
        throw SodiumNotAvailableException{};
      }

      // allocate the right type of memory
      if ((type == SodiumSecureMemType::Normal) || (type == SodiumSecureMemType::Locked))
      {
        rawPtr = malloc(len);
      }
      if (type == SodiumSecureMemType::Guarded)
      {
        rawPtr = lib->malloc(len);
      }

      // was the allocation successful?
      if (rawPtr == nullptr)
      {
        throw SodiumOutOfMemoryException{"ctor SodiumSecureMemory"};
      }

      // lock, if necessary
      if (type == SodiumSecureMemType::Locked)
      {
        int lockResult = lib->mlock(rawPtr, len);
        if (lockResult < 0)
        {
          free(rawPtr);
          throw SodiumOutOfMemoryException("cotr SodiumSecureMemory, could not lock memory");
        }
      }
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory::SodiumSecureMemory(const string& src, SodiumSecureMemType t)
      :SodiumSecureMemory{src.size(), t}
    {
      memcpy(rawPtr, src.c_str(), len);
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory::SodiumSecureMemory(SodiumSecureMemory&& other)
    {
      *this = std::move(other);
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory& SodiumSecureMemory::operator=(SodiumSecureMemory&& other)
    {
      // free my current ressources
      releaseMemory();

      // take over other's ressources
      rawPtr = other.rawPtr;
      len = other.len;
      type = other.type;
      lib = other.lib;
      curProtection = other.curProtection;

      // invalidate other's ressources
      other.rawPtr = nullptr;
      other.len = 0;
      other.lib = nullptr;

      return *this;
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory::~SodiumSecureMemory()
    {
      releaseMemory();
    }

    //----------------------------------------------------------------------------

    bool SodiumSecureMemory::setAccess(SodiumSecureMemAccess a)
    {
      if (type != SodiumSecureMemType::Guarded) return false;
      if (a == curProtection) return true;

      switch (a) {

      case SodiumSecureMemAccess::NoAccess:
      {
        bool isOkay = (lib->mprotect_noaccess(rawPtr) >= 0);
        if (isOkay) curProtection = SodiumSecureMemAccess::NoAccess;
        return isOkay;
      }

      case SodiumSecureMemAccess::RO:
      {
        bool isOkay = (lib->mprotect_readonly(rawPtr) >= 0);
        if (isOkay) curProtection = SodiumSecureMemAccess::RO;
        return isOkay;
      }

      case SodiumSecureMemAccess::RW:
      {
        bool isOkay = (lib->mprotect_readwrite(rawPtr) >= 0);
        if (isOkay) curProtection = SodiumSecureMemAccess::RW;
        return isOkay;
      }
      }

      return false;
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumSecureMemory::copy() const
    {
      return asCopy(*this);
    }

    //----------------------------------------------------------------------------

    void SodiumSecureMemory::shrink(size_t newSize)
    {
      // range check for the new size
      if ((newSize == 0) || (newSize >= len)) return;

      if (!(canRead()))
      {
        throw SodiumKeyLocked{"shrinking memory"};
      }

      // allocate new memory
      SodiumSecureMemory newMem{newSize, type};

      // copy the data over
      memcpy(newMem.rawPtr, rawPtr, newSize);

      // release the old memory and do the other
      // stuff by re-using the move operator
      *this = std::move(newMem);
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumSecureMemory::asCopy(const SodiumSecureMemory& src)
    {
      if (!(src.canRead()))
      {
        throw SodiumKeyLocked{"creating a key copy"};
      }

      // allocate the same amount and type of memory
      //
      // this might throw exceptions if we're running out of memory
      SodiumSecureMemory cpy{src.getSize(), src.getType()};

      SodiumSecureMemAccess oldProtection = src.getProtection();

      // do the copy itself
      memcpy(cpy.get(), src.get(), src.getSize());

      // re-lock the source
      if (src.getType() == SodiumSecureMemType::Guarded)
      {
        // apply the same protection to the copy
        bool isOkay = cpy.setAccess(oldProtection);
        if (!isOkay)
        {
          throw SodiumMemoryManagementException{"protecting the copy in asCopy() for an existing SodiumSecureMemory"};
        }
      }

      // return using move semantics
      return cpy;
    }

    //----------------------------------------------------------------------------

    void SodiumSecureMemory::releaseMemory()
    {
      if (rawPtr == nullptr)
      {
        len = 0;
        return;
      }

      if (type == SodiumSecureMemType::Normal)
      {
        lib->memzero(rawPtr, len);
      }

      if (type == SodiumSecureMemType::Locked)
      {
        lib->munlock(rawPtr, len);
      }

      if ((type == SodiumSecureMemType::Normal) || (type == SodiumSecureMemType::Locked))
      {
        free(rawPtr);
        rawPtr = nullptr;
      }
      if (type == SodiumSecureMemType::Guarded)
      {
        lib->free(rawPtr);
        rawPtr = nullptr;
      }

      if (rawPtr != nullptr)
      {
        abort();  // something went wrong with memory management
      }

      len = 0;
    }

    //----------------------------------------------------------------------------


    //----------------------------------------------------------------------------

    SodiumLib*SodiumLib::getInstance()
    {
      if (inst == nullptr)
      {
        // clear all old dynamic loader errors
        dlerror();

        // try to load the library
        void *handle = dlopen("libsodium.so", RTLD_LAZY);

        if (handle == nullptr)
        {
          string errMsg{dlerror()};
          cerr << "Error when loading libSodium: " << errMsg << endl;
          return nullptr;
        }

        // initialize the singleton
        SodiumLib* tmp = new SodiumLib(handle);
        inst = unique_ptr<SodiumLib>(tmp);
      }

      return inst.get();
    }

    //----------------------------------------------------------------------------

    SodiumLib::SodiumLib(void* _libHandle)
      :libHandle{_libHandle}
    {
      if (libHandle == nullptr)
      {
        throw std::invalid_argument("Nullptr lib handle!");
      }

      // initialize the lib pointers
      *(void **)(&(sodium.init)) = dlsym(libHandle, "sodium_init");
      *(void **)(&(sodium.bin2hex)) = dlsym(libHandle, "sodium_bin2hex");
      *(void **)(&(sodium.memcmp)) = dlsym(libHandle, "sodium_memcmp");
      *(void **)(&(sodium.isZero)) = dlsym(libHandle, "sodium_is_zero");
      *(void **)(&(sodium.increment)) = dlsym(libHandle, "sodium_increment");
      *(void **)(&(sodium.add)) = dlsym(libHandle, "sodium_add");
      *(void **)(&(sodium.memzero)) = dlsym(libHandle, "sodium_memzero");
      *(void **)(&(sodium.mlock)) = dlsym(libHandle, "sodium_mlock");
      *(void **)(&(sodium.munlock)) = dlsym(libHandle, "sodium_munlock");
      *(void **)(&(sodium.malloc)) = dlsym(libHandle, "sodium_malloc");
      *(void **)(&(sodium.allocarray)) = dlsym(libHandle, "sodium_allocarray");
      *(void **)(&(sodium.free)) = dlsym(libHandle, "sodium_free");
      *(void **)(&(sodium.mprotect_noaccess)) = dlsym(libHandle, "sodium_mprotect_noaccess");
      *(void **)(&(sodium.mprotect_readonly)) = dlsym(libHandle, "sodium_mprotect_readonly");
      *(void **)(&(sodium.mprotect_readwrite)) = dlsym(libHandle, "sodium_mprotect_readwrite");
      *(void **)(&(sodium.randombytes_random)) = dlsym(libHandle, "randombytes_random");
      *(void **)(&(sodium.randombytes_uniform)) = dlsym(libHandle, "randombytes_uniform");
      *(void **)(&(sodium.randombytes_buf)) = dlsym(libHandle, "randombytes_buf");
      *(void **)(&(sodium.crypto_secretbox_easy)) = dlsym(libHandle, "crypto_secretbox_easy");
      *(void **)(&(sodium.crypto_secretbox_open_easy)) = dlsym(libHandle, "crypto_secretbox_open_easy");
      *(void **)(&(sodium.crypto_secretbox_detached)) = dlsym(libHandle, "crypto_secretbox_detached");
      *(void **)(&(sodium.crypto_secretbox_open_detached)) = dlsym(libHandle, "crypto_secretbox_open_detached");
      *(void **)(&(sodium.crypto_auth)) = dlsym(libHandle, "crypto_auth");
      *(void **)(&(sodium.crypto_auth_verify)) = dlsym(libHandle, "crypto_auth_verify");
      *(void **)(&(sodium.crypto_aead_chacha20poly1305_encrypt)) = dlsym(libHandle, "crypto_aead_chacha20poly1305_encrypt");
      *(void **)(&(sodium.crypto_aead_chacha20poly1305_decrypt)) = dlsym(libHandle, "crypto_aead_chacha20poly1305_decrypt");
      *(void **)(&(sodium.crypto_aead_chacha20poly1305_encrypt_detached)) = dlsym(libHandle, "crypto_aead_chacha20poly1305_encrypt_detached");
      *(void **)(&(sodium.crypto_aead_chacha20poly1305_decrypt_detached)) = dlsym(libHandle, "crypto_aead_chacha20poly1305_decrypt_detached");
      *(void **)(&(sodium.crypto_aead_aes256gcm_is_available)) = dlsym(libHandle, "crypto_aead_aes256gcm_is_available");
      *(void **)(&(sodium.crypto_aead_aes256gcm_encrypt)) = dlsym(libHandle, "crypto_aead_aes256gcm_encrypt");
      *(void **)(&(sodium.crypto_aead_aes256gcm_decrypt)) = dlsym(libHandle, "crypto_aead_aes256gcm_decrypt");
      *(void **)(&(sodium.crypto_aead_aes256gcm_encrypt_detached)) = dlsym(libHandle, "crypto_aead_aes256gcm_encrypt_detached");
      *(void **)(&(sodium.crypto_aead_aes256gcm_decrypt_detached)) = dlsym(libHandle, "crypto_aead_aes256gcm_decrypt_detached");
      *(void **)(&(sodium.crypto_box_keypair)) = dlsym(libHandle, "crypto_box_keypair");
      *(void **)(&(sodium.crypto_box_seed_keypair)) = dlsym(libHandle, "crypto_box_seed_keypair");
      *(void **)(&(sodium.crypto_scalarmult_base)) = dlsym(libHandle, "crypto_scalarmult_base");
      *(void **)(&(sodium.crypto_box_easy)) = dlsym(libHandle, "crypto_box_easy");
      *(void **)(&(sodium.crypto_box_open_easy)) = dlsym(libHandle, "crypto_box_open_easy");
      *(void **)(&(sodium.crypto_box_detached)) = dlsym(libHandle, "crypto_box_detached");
      *(void **)(&(sodium.crypto_box_open_detached)) = dlsym(libHandle, "crypto_box_open_detached");
      *(void **)(&(sodium.crypto_sign_keypair)) = dlsym(libHandle, "crypto_sign_keypair");
      *(void **)(&(sodium.crypto_sign_seed_keypair)) = dlsym(libHandle, "crypto_sign_seed_keypair");
      *(void **)(&(sodium.crypto_sign)) = dlsym(libHandle, "crypto_sign");
      *(void **)(&(sodium.crypto_sign_open)) = dlsym(libHandle, "crypto_sign_open");
      *(void **)(&(sodium.crypto_sign_detached)) = dlsym(libHandle, "crypto_sign_detached");
      *(void **)(&(sodium.crypto_sign_verify_detached)) = dlsym(libHandle, "crypto_sign_verify_detached");
      *(void **)(&(sodium.crypto_sign_ed25519_sk_to_seed)) = dlsym(libHandle, "crypto_sign_ed25519_sk_to_seed");
      *(void **)(&(sodium.crypto_sign_ed25519_sk_to_pk)) = dlsym(libHandle, "crypto_sign_ed25519_sk_to_pk");
      //*(void **)(&(sodium.)) = dlsym(libHandle, "sodium_");

      // make sure we've successfully loaded all symbols
      if ((sodium.init == nullptr) ||
          (sodium.bin2hex == nullptr) ||
          (sodium.memcmp == nullptr) ||
          (sodium.isZero == nullptr) ||
          (sodium.increment == nullptr) ||
          (sodium.add == nullptr) ||
          (sodium.memzero == nullptr) ||
          (sodium.mlock == nullptr) ||
          (sodium.munlock == nullptr) ||
          (sodium.malloc == nullptr) ||
          (sodium.allocarray == nullptr) ||
          (sodium.free == nullptr) ||
          (sodium.mprotect_noaccess == nullptr) ||
          (sodium.mprotect_readonly == nullptr) ||
          (sodium.mprotect_readwrite == nullptr) ||
          (sodium.randombytes_random == nullptr) ||
          (sodium.randombytes_uniform == nullptr) ||
          (sodium.randombytes_buf == nullptr) ||
          (sodium.crypto_secretbox_easy == nullptr) ||
          (sodium.crypto_secretbox_open_easy == nullptr) ||
          (sodium.crypto_secretbox_detached == nullptr) ||
          (sodium.crypto_secretbox_open_detached == nullptr) ||
          (sodium.crypto_auth == nullptr) ||
          (sodium.crypto_auth_verify == nullptr) ||
          (sodium.crypto_aead_chacha20poly1305_encrypt == nullptr) ||
          (sodium.crypto_aead_chacha20poly1305_decrypt == nullptr) ||
          (sodium.crypto_aead_chacha20poly1305_encrypt_detached == nullptr) ||
          (sodium.crypto_aead_chacha20poly1305_decrypt_detached == nullptr) ||
          (sodium.crypto_aead_aes256gcm_is_available == nullptr) ||
          (sodium.crypto_aead_aes256gcm_encrypt == nullptr) ||
          (sodium.crypto_aead_aes256gcm_decrypt == nullptr) ||
          (sodium.crypto_aead_aes256gcm_encrypt_detached == nullptr) ||
          (sodium.crypto_aead_aes256gcm_decrypt_detached == nullptr) ||
          (sodium.crypto_box_keypair == nullptr) ||
          (sodium.crypto_box_seed_keypair == nullptr) ||
          (sodium.crypto_scalarmult_base == nullptr) ||
          (sodium.crypto_box_easy == nullptr) ||
          (sodium.crypto_box_open_easy == nullptr) ||
          (sodium.crypto_box_detached == nullptr) ||
          (sodium.crypto_box_open_detached == nullptr) ||
          (sodium.crypto_sign_keypair == nullptr) ||
          (sodium.crypto_sign_seed_keypair == nullptr) ||
          (sodium.crypto_sign == nullptr) ||
          (sodium.crypto_sign_open == nullptr) ||
          (sodium.crypto_sign_detached == nullptr) ||
          (sodium.crypto_sign_verify_detached == nullptr) ||
          (sodium.crypto_sign_ed25519_sk_to_seed == nullptr) ||
          (sodium.crypto_sign_ed25519_sk_to_pk == nullptr)
          //(sodium.crypto_aead_chacha20poly1305 == nullptr) ||
          )
      {
        dlclose(libHandle);
        libHandle = nullptr;
        throw std::runtime_error("Could not load all libsodium symbols!");
      }

      // initialize libsodium
      int iniResult = sodium.init();
      cerr << " libsodium init result = " << iniResult << endl;
      if (iniResult < 0)
      {
        dlclose(libHandle);
        libHandle = nullptr;
        throw std::runtime_error("sodium_init failed");
      }
      if (iniResult == 0)
      {
        cout << "libsodium successfully initialized!" << endl;
      }
      if (iniResult > 0)
      {
        cout << "libsodium already initialized before..." << endl;
      }
    }

    //----------------------------------------------------------------------------

    SodiumLib::~SodiumLib()
    {
      // unload the library
      if (libHandle != nullptr) dlclose(libHandle);

      cerr << "Wrapper for libsodium unloaded!" << endl;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::memcmp(const ManagedMemory& b1, const ManagedMemory& b2) const
    {
      if (b1.getSize() != b2.getSize()) return false;

      return (sodium.memcmp(b1.get(), b2.get(), b1.getSize()) == 0);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::bin2hex(const string& binData) const
    {
      char result[binData.size() * 2 + 1];

      // prepare a result string with 2 x hexDataLength + 1 bytes
      sodium.bin2hex(result, sizeof result, (unsigned char*)(binData.c_str()), binData.size());

      return string{result};
    }

    //----------------------------------------------------------------------------

    string SodiumLib::bin2hex(const ManagedBuffer& binData) const
    {
      char result[binData.getSize() * 2 + 1];

      // prepare a result string with 2 x hexDataLength + 1 bytes
      sodium.bin2hex(result, sizeof result, binData.get_uc(), binData.getSize());

      return string{result};
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::isZero(const ManagedMemory& buf) const
    {
      return (sodium.isZero(buf.get_uc(), buf.getSize()) == 1);
    }

    //----------------------------------------------------------------------------

    void SodiumLib::increment(const ManagedMemory& buf)
    {
      sodium.increment(buf.get_uc(), buf.getSize());
    }

    //----------------------------------------------------------------------------

    void SodiumLib::add(const ManagedMemory& a, const ManagedMemory& b)
    {
      if (a.getSize() != b.getSize())
      {
        throw SodiumInvalidKeysize{"the size of two large numbers for adding did not match"};
      }

      sodium.add(a.get_uc(), b.get_uc(), b.getSize());
    }

    //----------------------------------------------------------------------------

    void SodiumLib::memzero(void* const pnt, const size_t len)
    {
      sodium.memzero(pnt, len);
    }

    //----------------------------------------------------------------------------

    int SodiumLib::mlock(void* const addr, const size_t len)
    {
      return sodium.mlock(addr, len);
    }

    //----------------------------------------------------------------------------

    int SodiumLib::munlock(void* const addr, const size_t len)
    {
      return sodium.munlock(addr, len);
    }

    //----------------------------------------------------------------------------

    void* SodiumLib::malloc(size_t size)
    {
      return sodium.malloc(size);
    }

    //----------------------------------------------------------------------------

    void* SodiumLib::allocarray(size_t count, size_t size)
    {
      return sodium.allocarray(count, size);
    }

    //----------------------------------------------------------------------------

    void SodiumLib::free(void* ptr)
    {
      sodium.free(ptr);
    }

    //----------------------------------------------------------------------------

    int SodiumLib::mprotect_noaccess(void* ptr)
    {
      return sodium.mprotect_noaccess(ptr);
    }

    //----------------------------------------------------------------------------

    int SodiumLib::mprotect_readonly(void* ptr)
    {
      return sodium.mprotect_readonly(ptr);
    }

    //----------------------------------------------------------------------------

    int SodiumLib::mprotect_readwrite(void* ptr)
    {
      return sodium.mprotect_readwrite(ptr);
    }

    //----------------------------------------------------------------------------

    uint32_t SodiumLib::randombytes_random() const
    {
      return sodium.randombytes_random();
    }

    //----------------------------------------------------------------------------

    uint32_t SodiumLib::randombytes_uniform(const uint32_t upper_bound) const
    {
      return sodium.randombytes_uniform(upper_bound);
    }

    //----------------------------------------------------------------------------

    void SodiumLib::randombytes_buf(const ManagedMemory& buf) const
    {
      sodium.randombytes_buf(buf.get(), buf.getSize());
    }

    //----------------------------------------------------------------------------

    ManagedBuffer SodiumLib::crypto_secretbox_easy(const ManagedMemory& msg, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key)
    {
      // the message should be valid
      if (!(msg.isValid())) return ManagedBuffer{};
      if (!(nonce.isValid())) return ManagedBuffer{};
      if (!(key.isValid())) return ManagedBuffer{};

      // allocate space for the result
      ManagedBuffer cipher{crypto_secretbox_MACBYTES + msg.getSize()};

      // do the actual encryption
      sodium.crypto_secretbox_easy(cipher.get_uc(), msg.get_uc(), msg.getSize(), nonce.get_uc(), key.get_uc());

      // return the result
      return cipher;
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::crypto_secretbox_open_easy(const ManagedMemory& cipher, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key, SodiumSecureMemType clearTextProtection)
    {
      // the cipher should be valid
      if (!(cipher.isValid())) return SodiumSecureMemory{};
      if (!(nonce.isValid())) return SodiumSecureMemory{};
      if (!(key.isValid())) return SodiumSecureMemory{};

      // we need at least one message byte, so just the MAC is
      // not sufficient
      if (cipher.getSize() <= crypto_secretbox_MACBYTES) return SodiumSecureMemory{};

      // allocate space for the result
      SodiumSecureMemory msg{cipher.getSize() - crypto_secretbox_MACBYTES, clearTextProtection};

      // decrypt
      int isOkay = sodium.crypto_secretbox_open_easy(msg.get_uc(), cipher.get_uc(), cipher.getSize(), nonce.get_uc(), key.get_uc());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? std::move(msg) : SodiumSecureMemory{};
    }

    //----------------------------------------------------------------------------

    pair<ManagedBuffer, ManagedBuffer> SodiumLib::crypto_secretbox_detached(const ManagedMemory& msg, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key)
    {
      pair<ManagedBuffer, ManagedBuffer> errorResult = make_pair(ManagedBuffer{}, ManagedBuffer{});

      // the message should be valid
      if (!(msg.isValid())) return errorResult;
      if (!(nonce.isValid())) return errorResult;
      if (!(key.isValid())) return errorResult;

      // allocate space for the result
      ManagedBuffer cipher{msg.getSize()};
      ManagedBuffer mac{crypto_secretbox_MACBYTES};

      // do the actual encryption
      sodium.crypto_secretbox_detached(cipher.get_uc(), mac.get_uc(), msg.get_uc(), msg.getSize(), nonce.get_uc(), key.get_uc());

      // return the result
      return make_pair(std::move(cipher), std::move(mac));
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::crypto_secretbox_open_detached(const ManagedMemory& cipher, const ManagedMemory& mac, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key, SodiumSecureMemType clearTextProtection)
    {
      // the cipher should be valid
      if (!(cipher.isValid())) return SodiumSecureMemory{};
      if (!(mac.isValid())) return SodiumSecureMemory{};
      if (!(nonce.isValid())) return SodiumSecureMemory{};
      if (!(key.isValid())) return SodiumSecureMemory{};

      // the MAC size should fit
      if (mac.getSize() != crypto_secretbox_MACBYTES) return SodiumSecureMemory{};

      // allocate space for the result
      SodiumSecureMemory msg{cipher.getSize(), clearTextProtection};

      // decrypt
      int isOkay = sodium.crypto_secretbox_open_detached(msg.get_uc(), cipher.get_uc(), mac.get_uc(), cipher.getSize(), nonce.get_uc(), key.get_uc());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? std::move(msg) : SodiumSecureMemory{};
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_secretbox_easy(const string& msg, const string& nonce, const string& key)
    {
      if (msg.empty()) return string{};

      // make sure that nonce and key have the correct length
      if (nonce.size() != crypto_secretbox_NONCEBYTES) throw SodiumInvalidKeysize{"nonce for secretbox"};
      if (key.size() != crypto_secretbox_KEYBYTES) throw SodiumInvalidKeysize{"key for secretbox"};

      // allocate space for the result
      string cipher;
      cipher.resize(crypto_secretbox_MACBYTES + msg.size());

      // do the actual encryption
      sodium.crypto_secretbox_easy((unsigned char *)cipher.c_str(),
                                   (const unsigned char *)(msg.c_str()), msg.size(),
                                   (const unsigned char *)(nonce.c_str()),
                                   (const unsigned char *)(key.c_str()));

      // return the result
      return cipher;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_secretbox_open_easy(const string& cipher, const string& nonce, const string& key)
    {
      // we need at least one message byte, so just the MAC is
      // not sufficient
      if (cipher.size() <= crypto_secretbox_MACBYTES) return string{};

      // make sure that nonce and key have the correct length
      if (nonce.size() != crypto_secretbox_NONCEBYTES) throw SodiumInvalidKeysize{"nonce for secretbox"};
      if (key.size() != crypto_secretbox_KEYBYTES) throw SodiumInvalidKeysize{"key for secretbox"};

      // allocate space for the result
      string msg;
      msg.resize(cipher.size() - crypto_secretbox_MACBYTES);

      // decrypt
      int isOkay = sodium.crypto_secretbox_open_easy((unsigned char *)msg.c_str(),
                                                     (const unsigned char *)cipher.c_str(), cipher.size(),
                                                     (const unsigned char *)nonce.c_str(),
                                                     (const unsigned char *)key.c_str());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? msg : string{};
    }

    //----------------------------------------------------------------------------

    pair<string, string> SodiumLib::crypto_secretbox_detached(const string& msg, const string& nonce, const string& key)
    {
      pair<string, string> errorResult = make_pair(string{}, string{});

      // the message should be valid
      if (msg.empty())  return errorResult;

      // make sure that nonce and key have the correct length
      if (nonce.size() != crypto_secretbox_NONCEBYTES) throw SodiumInvalidKeysize{"nonce for secretbox"};
      if (key.size() != crypto_secretbox_KEYBYTES) throw SodiumInvalidKeysize{"key for secretbox"};

      // allocate space for the result
      string cipher;
      cipher.resize(msg.size());
      string mac;
      mac.resize(crypto_secretbox_MACBYTES);

      // do the actual encryption
      sodium.crypto_secretbox_detached((unsigned char *)cipher.c_str(), (unsigned char *)mac.c_str(),
                                       (const unsigned char *)msg.c_str(), msg.size(),
                                       (const unsigned char *)nonce.c_str(),
                                       (const unsigned char *)key.c_str());

      // return the result
      return make_pair(cipher, mac);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_secretbox_open_detached(const string& cipher, const string& mac, const string& nonce, const string& key)
    {
      // the cipher should be valid
      if (cipher.empty())  return string{};

      // the MAC size should fit
      if (mac.size() != crypto_secretbox_MACBYTES) return string{};

      // make sure that nonce and key have the correct length
      if (nonce.size() != crypto_secretbox_NONCEBYTES) throw SodiumInvalidKeysize{"nonce for secretbox"};
      if (key.size() != crypto_secretbox_KEYBYTES) throw SodiumInvalidKeysize{"key for secretbox"};

      // allocate space for the result
      string msg;
      msg.resize(cipher.size());

      // decrypt
      int isOkay = sodium.crypto_secretbox_open_detached((unsigned char *)msg.c_str(),
                                                         (const unsigned char *)cipher.c_str(),
                                                         (const unsigned char *)mac.c_str(), cipher.size(),
                                                         (const unsigned char *)nonce.c_str(),
                                                         (const unsigned char *)key.c_str());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? msg : string{};
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_secretbox_easy(const string& msg, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key)
    {
      if (msg.empty()) return string{};
      if (!(nonce.isValid())) return string{};
      if (!(key.isValid())) return string{};

      // allocate space for the result
      string cipher;
      cipher.resize(crypto_secretbox_MACBYTES + msg.size());

      // do the actual encryption
      sodium.crypto_secretbox_easy((unsigned char *)cipher.c_str(),
                                   (const unsigned char *)(msg.c_str()), msg.size(),
                                   nonce.get_uc(), key.get_uc());

      // return the result
      return cipher;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_secretbox_open_easy(const string& cipher, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key)
    {
      if (!(nonce.isValid())) return string{};
      if (!(key.isValid())) return string{};

      // we need at least one message byte, so just the MAC is
      // not sufficient
      if (cipher.size() <= crypto_secretbox_MACBYTES) return string{};

      // allocate space for the result
      string msg;
      msg.resize(cipher.size() - crypto_secretbox_MACBYTES);

      // decrypt
      int isOkay = sodium.crypto_secretbox_open_easy((unsigned char *)msg.c_str(),
                                                     (const unsigned char *)cipher.c_str(), cipher.size(),
                                                     nonce.get_uc(), key.get_uc());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? msg : string{};
    }

    //----------------------------------------------------------------------------

    pair<string, string> SodiumLib::crypto_secretbox_detached(const string& msg, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key)
    {
      pair<string, string> errorResult = make_pair(string{}, string{});

      // the message should be valid
      if (msg.empty())  return errorResult;
      if (!(nonce.isValid())) return errorResult;
      if (!(key.isValid())) return errorResult;

      // allocate space for the result
      string cipher;
      cipher.resize(msg.size());
      string mac;
      mac.resize(crypto_secretbox_MACBYTES);

      // do the actual encryption
      sodium.crypto_secretbox_detached((unsigned char *)cipher.c_str(), (unsigned char *)mac.c_str(),
                                       (const unsigned char *)msg.c_str(), msg.size(),
                                       nonce.get_uc(), key.get_uc());

      // return the result
      return make_pair(cipher, mac);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_secretbox_open_detached(const string& cipher, const string& mac, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key)
    {
      if (!(nonce.isValid())) return string{};
      if (!(key.isValid())) return string{};

      // the cipher should be valid
      if (cipher.empty())  return string{};

      // the MAC size should fit
      if (mac.size() != crypto_secretbox_MACBYTES) return string{};

      // allocate space for the result
      string msg;
      msg.resize(cipher.size());

      // decrypt
      int isOkay = sodium.crypto_secretbox_open_detached((unsigned char *)msg.c_str(),
                                                         (const unsigned char *)cipher.c_str(),
                                                         (const unsigned char *)mac.c_str(), cipher.size(),
                                                         nonce.get_uc(), key.get_uc());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? msg  : string{};
    }

    //----------------------------------------------------------------------------

    SodiumLib::AuthTagType SodiumLib::crypto_auth(const ManagedMemory& msg, const SodiumLib::AuthKeyType& key)
    {
      if (!(msg.isValid())) return AuthTagType{};
      if (!(key.isValid())) return AuthTagType{};

      AuthTagType result;
      sodium.crypto_auth(result.get_uc(), msg.get_uc(), msg.getSize(), key.get_uc());

      return result;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::crypto_auth_verify(const ManagedMemory& msg, const SodiumLib::AuthTagType& tag, const SodiumLib::AuthKeyType& key)
    {
      if (!(msg.isValid())) return false;
      if (!(tag.isValid())) return false;
      if (!(key.isValid())) return false;

      return (sodium.crypto_auth_verify(tag.get_uc(), msg.get_uc(), msg.getSize(), key.get_uc()) == 0);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_auth(const string& msg, const string& key)
    {
      // check input data validiy
      if (msg.empty()) return string{};
      if (key.size() != crypto_auth_KEYBYTES) return string{};

      // allocate space for the result
      string tag;
      tag.resize(crypto_auth_BYTES);

      // hash
      sodium.crypto_auth((unsigned char*)tag.c_str(), (const unsigned char*) msg.c_str(), msg.size(), (const unsigned char*)key.c_str());

      return tag;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::crypto_auth_verify(const string& msg, const string& tag, const string& key)
    {
      if (msg.empty()) return false;
      if (tag.size() != crypto_auth_BYTES) return false;
      if (key.size() != crypto_auth_KEYBYTES) return false;

      int rc = sodium.crypto_auth_verify((const unsigned char*)tag.c_str(), (const unsigned char*)msg.c_str(), msg.size(),
                                         (const unsigned char*)key.c_str());

      return (rc == 0);
    }

    //----------------------------------------------------------------------------

    ManagedBuffer SodiumLib::crypto_aead_encrypt(int (*funcPtr)(unsigned char*, unsigned long long*, const unsigned char*, unsigned long long, const unsigned char*,
                                                                unsigned long long, const unsigned char*, const unsigned char*, const unsigned char*), size_t tagSize,
                                                 const ManagedMemory& msg, const ManagedMemory& nonce, const ManagedMemory& key, const ManagedBuffer& ad)
    {
      if (funcPtr == nullptr) return ManagedBuffer{};

      if (!(msg.isValid())) return ManagedBuffer{};
      if (!(nonce.isValid())) return ManagedBuffer{};
      if (!(key.isValid())) return ManagedBuffer{};

      // alocate space for the result
      size_t maxCipherLen = msg.getSize() + tagSize;
      ManagedBuffer cipher{maxCipherLen};

      // do we use additional data?
      unsigned char *adPtr = nullptr;
      size_t adLen = 0;
      if (ad.isValid())
      {
        adPtr = ad.get_uc();
        adLen = ad.getSize();
      }

      // call the encryption function
      unsigned long long actualCipherLen = 0;
      funcPtr(cipher.get_uc(), &actualCipherLen, msg.get_uc(), msg.getSize(),
              adPtr, adLen, nullptr, nonce.get_uc(), key.get_uc());

      // do we need to shrink the result?
      if (actualCipherLen < maxCipherLen)
      {
        cipher.shrink(actualCipherLen);
      }

      return cipher;
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::crypto_aead_decrypt(int (*funcPtr)(unsigned char*, unsigned long long*, unsigned char*, const unsigned char*, unsigned long long, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*), size_t tagSize, const ManagedMemory& cipher, const ManagedMemory& nonce, const ManagedMemory& key, const ManagedBuffer& ad, SodiumSecureMemType clearTextProtection)
    {
      if (!(cipher.isValid())) return SodiumSecureMemory{};
      if (!(nonce.isValid())) return SodiumSecureMemory{};
      if (!(key.isValid())) return SodiumSecureMemory{};

      // we need to have at least one byte of message data besides the tag
      if (cipher.getSize() <= tagSize) return SodiumSecureMemory{};

      // calc the maximum message size
      size_t maxMsgLen = cipher.getSize() - tagSize;
      SodiumSecureMemory msg{maxMsgLen, clearTextProtection};

      // do we use additional data?
      unsigned char *adPtr = nullptr;
      size_t adLen = 0;
      if (ad.isValid())
      {
        adPtr = ad.get_uc();
        adLen = ad.getSize();
      }

      // call the decryption function
      unsigned long long actualMsgLen = 0;
      int rc = funcPtr(msg.get_uc(), &actualMsgLen, nullptr, cipher.get_uc(), cipher.getSize(),
                                                  adPtr, adLen, nonce.get_uc(), key.get_uc());

      if (rc != 0) return SodiumSecureMemory{};  // verification failed

      // do we need to shrink the result?
      if (actualMsgLen < maxMsgLen)
      {
        msg.shrink(actualMsgLen);
      }

      return msg;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_aead_encrypt(int (*funcPtr)(unsigned char*, unsigned long long*, const unsigned char*, unsigned long long, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*, const unsigned char*), size_t nonceSize, size_t keySize, size_t tagSize, const string& msg, const string& nonce, const string& key, const string& ad)
    {
      // check parameter validity
      if (msg.empty()) return string{};
      if (nonce.size() != nonceSize) return string{};
      if (key.size() != keySize) return string{};

      // alocate space for the result
      size_t maxCipherLen = msg.size() + tagSize;
      string cipher;
      cipher.resize(maxCipherLen);

      // do we use additional data?
      unsigned char *adPtr = nullptr;
      size_t adLen = 0;
      if (!(ad.empty()))
      {
        adPtr = (unsigned char*)ad.c_str();
        adLen = ad.size();
      }

      // call the encryption function
      unsigned long long actualCipherLen = 0;
      funcPtr((unsigned char*)cipher.c_str(), &actualCipherLen, (unsigned char*)msg.c_str(), msg.size(),
              adPtr, adLen, nullptr, (unsigned char*)nonce.c_str(), (unsigned char*)key.c_str());

      // do we need to shrink the result?
      if (actualCipherLen < maxCipherLen)
      {
        cipher = cipher.substr(0, actualCipherLen);
      }

      return cipher;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_aead_decrypt(int (*funcPtr)(unsigned char*, unsigned long long*, unsigned char*, const unsigned char*,
                                                         unsigned long long, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*),
                                          size_t nonceSize, size_t keySize, size_t tagSize, const string& cipher,
                                          const string& nonce, const string& key, const string& ad)
    {
      // check parameter validity
      if (cipher.size() <= tagSize) return string{};  // require at least one message byte
      if (nonce.size() != nonceSize) return string{};
      if (key.size() != keySize) return string{};

      // calc the maximum message size
      size_t maxMsgLen = cipher.size() - tagSize;
      string msg;
      msg.resize(maxMsgLen);

      // do we use additional data?
      unsigned char *adPtr = nullptr;
      size_t adLen = 0;
      if (!(ad.empty()))
      {
        adPtr = (unsigned char*)ad.c_str();
        adLen = ad.size();
      }

      // call the decryption function
      unsigned long long actualMsgLen = 0;
      int rc = funcPtr((unsigned char*)msg.c_str(), &actualMsgLen, nullptr,
                       (unsigned char*)cipher.c_str(), cipher.size(), adPtr, adLen,
                       (unsigned char*)nonce.c_str(), (unsigned char*)key.c_str());

      if (rc != 0) return string{};  // verification failed

      // do we need to shrink the result?
      if (actualMsgLen < maxMsgLen)
      {
        msg = msg.substr(0, actualMsgLen);
      }

      return msg;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_aead_encrypt(int (*funcPtr)(unsigned char*, unsigned long long*, const unsigned char*, unsigned long long, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*, const unsigned char*), size_t tagSize, const string& msg, const ManagedMemory& nonce, const ManagedMemory& key, const string& ad)
    {
      // check parameter validity
      if (msg.empty()) return string{};
      if (!(nonce.isValid())) return string{};
      if (!(key.isValid())) return string{};

      // alocate space for the result
      size_t maxCipherLen = msg.size() + tagSize;
      string cipher;
      cipher.resize(maxCipherLen);

      // do we use additional data?
      unsigned char *adPtr = nullptr;
      size_t adLen = 0;
      if (!(ad.empty()))
      {
        adPtr = (unsigned char*)ad.c_str();
        adLen = ad.size();
      }

      // call the encryption function
      unsigned long long actualCipherLen = 0;
      funcPtr((unsigned char*)cipher.c_str(), &actualCipherLen,
              (unsigned char*)msg.c_str(), msg.size(), adPtr, adLen,
              nullptr, nonce.get_uc(), key.get_uc());

      // do we need to shrink the result?
      if (actualCipherLen < maxCipherLen)
      {
        cipher = cipher.substr(0, actualCipherLen);
      }

      return cipher;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_aead_decrypt(int (*funcPtr)(unsigned char*, unsigned long long*, unsigned char*, const unsigned char*, unsigned long long, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*), size_t tagSize, const string& cipher, const ManagedMemory& nonce, const ManagedMemory& key, const string& ad)
    {
      // check parameter validity
      if (cipher.size() <= tagSize) return string{};  // require at least one message byte
      if (!(nonce.isValid())) return string{};
      if (!(key.isValid())) return string{};

      // calc the maximum message size
      size_t maxMsgLen = cipher.size() - tagSize;
      string msg;
      msg.resize(maxMsgLen);

      // do we use additional data?
      unsigned char *adPtr = nullptr;
      size_t adLen = 0;
      if (!(ad.empty()))
      {
        adPtr = (unsigned char*)ad.c_str();
        adLen = ad.size();
      }

      // call the decryption function
      unsigned long long actualMsgLen = 0;
      int rc = funcPtr((unsigned char*)msg.c_str(), &actualMsgLen,
                       nullptr, (unsigned char*)cipher.c_str(), cipher.size(),
                       adPtr, adLen, nonce.get_uc(), key.get_uc());

      if (rc != 0) return string{};  // verification failed

      // do we need to shrink the result?
      if (actualMsgLen < maxMsgLen)
      {
        msg = msg.substr(0, actualMsgLen);
      }

      return msg;
    }

    //----------------------------------------------------------------------------

    ManagedBuffer SodiumLib::crypto_aead_chacha20poly1305_encrypt(const ManagedMemory& msg,
                                                                  const SodiumLib::AEAD_ChaCha20Poly1305_NonceType& nonce, const SodiumLib::AEAD_ChaCha20Poly1305_KeyType& key,
                                                                  const ManagedBuffer& ad)
    {
      return crypto_aead_encrypt(sodium.crypto_aead_chacha20poly1305_encrypt, crypto_aead_chacha20poly1305_ABYTES, msg, nonce, key, ad);
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::crypto_aead_chacha20poly1305_decrypt(const ManagedMemory& cipher, const SodiumLib::AEAD_ChaCha20Poly1305_NonceType& nonce,
                                                                       const SodiumLib::AEAD_ChaCha20Poly1305_KeyType& key, const ManagedBuffer& ad,
                                                                       SodiumSecureMemType clearTextProtection)
    {
      return crypto_aead_decrypt(sodium.crypto_aead_chacha20poly1305_decrypt, crypto_aead_chacha20poly1305_ABYTES, cipher, nonce, key, ad, clearTextProtection);
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::is_AES256GCM_avail()
    {
      return (sodium.crypto_aead_aes256gcm_is_available() == 1);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_aead_chacha20poly1305_encrypt(const string& msg, const string& nonce, const string& key, const string& ad)
    {
      return crypto_aead_encrypt(sodium.crypto_aead_chacha20poly1305_encrypt, crypto_aead_chacha20poly1305_NPUBBYTES, crypto_aead_chacha20poly1305_KEYBYTES,
                                 crypto_aead_chacha20poly1305_KEYBYTES, msg, nonce, key, ad);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_aead_chacha20poly1305_decrypt(const string& cipher, const string& nonce, const string& key, const string& ad)
    {
      return crypto_aead_decrypt(sodium.crypto_aead_chacha20poly1305_decrypt, crypto_aead_chacha20poly1305_NPUBBYTES,
                                 crypto_aead_chacha20poly1305_KEYBYTES, crypto_aead_chacha20poly1305_ABYTES, cipher,
                                 nonce, key, ad);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_aead_chacha20poly1305_encrypt(const string& msg, const SodiumLib::AEAD_ChaCha20Poly1305_NonceType& nonce, const SodiumLib::AEAD_ChaCha20Poly1305_KeyType& key, const string& ad)
    {
      return crypto_aead_encrypt(sodium.crypto_aead_chacha20poly1305_encrypt, crypto_aead_chacha20poly1305_ABYTES, msg, nonce, key, ad);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_aead_chacha20poly1305_decrypt(const string& cipher, const SodiumLib::AEAD_ChaCha20Poly1305_NonceType& nonce, const SodiumLib::AEAD_ChaCha20Poly1305_KeyType& key, const string& ad)
    {
      return crypto_aead_decrypt(sodium.crypto_aead_chacha20poly1305_decrypt, crypto_aead_chacha20poly1305_ABYTES,
                                 cipher, nonce, key, ad);
    }

    //----------------------------------------------------------------------------

    ManagedBuffer SodiumLib::crypto_aead_aes256gcm_encrypt(const ManagedMemory& msg, const SodiumLib::AEAD_AES256GCM_NonceType& nonce, const SodiumLib::AEAD_AES256GCM_KeyType& key, const ManagedBuffer& ad)
    {
      if (sodium.crypto_aead_aes256gcm_is_available() != 1) return ManagedBuffer{};

      return crypto_aead_encrypt(sodium.crypto_aead_aes256gcm_encrypt, crypto_aead_aes256gcm_ABYTES, msg, nonce, key, ad);
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::crypto_aead_aes256gcm_decrypt(const ManagedMemory& cipher, const SodiumLib::AEAD_AES256GCM_NonceType& nonce, const SodiumLib::AEAD_AES256GCM_KeyType& key, const ManagedBuffer& ad, SodiumSecureMemType clearTextProtection)
    {
      if (sodium.crypto_aead_aes256gcm_is_available() != 1) return SodiumSecureMemory{};

      return crypto_aead_decrypt(sodium.crypto_aead_aes256gcm_decrypt, crypto_aead_aes256gcm_ABYTES, cipher, nonce, key, ad, clearTextProtection);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_aead_aes256gcm_encrypt(const string& msg, const string& nonce, const string& key, const string& ad)
    {
      if (sodium.crypto_aead_aes256gcm_is_available() != 1) return string{};

      return crypto_aead_encrypt(sodium.crypto_aead_aes256gcm_encrypt, crypto_aead_aes256gcm_NPUBBYTES, crypto_aead_aes256gcm_KEYBYTES,
                                 crypto_aead_aes256gcm_ABYTES, msg, nonce, key, ad);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_aead_aes256gcm_decrypt(const string& cipher, const string& nonce, const string& key, const string& ad)
    {
      if (sodium.crypto_aead_aes256gcm_is_available() != 1) return string{};

      return crypto_aead_decrypt(sodium.crypto_aead_aes256gcm_decrypt, crypto_aead_aes256gcm_NPUBBYTES, crypto_aead_aes256gcm_KEYBYTES,
                                 crypto_aead_aes256gcm_ABYTES, cipher, nonce, key, ad);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_aead_aes256gcm_encrypt(const string& msg, const SodiumLib::AEAD_AES256GCM_NonceType& nonce, const SodiumLib::AEAD_AES256GCM_KeyType& key, const string& ad)
    {
      if (sodium.crypto_aead_aes256gcm_is_available() != 1) return string{};

      return crypto_aead_encrypt(sodium.crypto_aead_aes256gcm_encrypt, crypto_aead_aes256gcm_ABYTES, msg, nonce, key, ad);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_aead_aes256gcm_decrypt(const string& cipher, const SodiumLib::AEAD_AES256GCM_NonceType& nonce, const SodiumLib::AEAD_AES256GCM_KeyType& key, const string& ad)
    {
      if (sodium.crypto_aead_aes256gcm_is_available() != 1) return string{};

      return crypto_aead_decrypt(sodium.crypto_aead_aes256gcm_decrypt, crypto_aead_aes256gcm_ABYTES, cipher, nonce, key, ad);
    }

    //----------------------------------------------------------------------------

    void SodiumLib::genAsymCryptoKeyPair(AsymCrypto_PublicKey& pk_out, AsymCrypto_SecretKey& sk_out)
    {
      sodium.crypto_box_keypair(pk_out.get_uc(), sk_out.get_uc());
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::genAsymCryptoKeyPairSeeded(const SodiumLib::AsymCrypto_KeySeed& seed, AsymCrypto_PublicKey& pk_out, AsymCrypto_SecretKey& sk_out)
    {
      if (!(seed.isValid()))
      {
        return false;
      }

      sodium.crypto_box_seed_keypair(pk_out.get_uc(), sk_out.get_uc(), seed.get_uc());

      return true;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::genPublicCryptoKeyFromSecretKey(const SodiumLib::AsymCrypto_SecretKey& sk, AsymCrypto_PublicKey& pk_out)
    {
      if (!(sk.isValid()))
      {
        return false;
      }

      sodium.crypto_scalarmult_base(pk_out.get_uc(), sk.get_uc());

      return true;
    }

    //----------------------------------------------------------------------------

    ManagedBuffer SodiumLib::crypto_box_easy(const ManagedMemory& msg, const SodiumLib::AsymCrypto_Nonce& nonce,
                                             const SodiumLib::AsymCrypto_PublicKey& recipientKey, const SodiumLib::AsymCrypto_SecretKey& senderKey)
    {
      if (!(msg.isValid())) return ManagedBuffer{};
      if (!(nonce.isValid())) return ManagedBuffer{};
      if (!(recipientKey.isValid())) return ManagedBuffer{};
      if (!(senderKey.isValid())) return ManagedBuffer{};

      // allocate space for the result
      ManagedBuffer cipher{msg.getSize() + crypto_box_MACBYTES};

      // the actual encryption
      sodium.crypto_box_easy(cipher.get_uc(), msg.get_uc(), msg.getSize(), nonce.get_uc(),
                             recipientKey.get_uc(), senderKey.get_uc());

      return cipher;
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::crypto_box_open_easy(const ManagedMemory& cipher, const SodiumLib::AsymCrypto_Nonce& nonce,
                                                       const SodiumLib::AsymCrypto_PublicKey& senderKey, const SodiumLib::AsymCrypto_SecretKey& recipientKey,
                                                       SodiumSecureMemType clearTextProtection)
    {
      // the cipher and keys should be valid
      if (!(cipher.isValid())) return SodiumSecureMemory{};
      if (!(nonce.isValid())) return SodiumSecureMemory{};
      if (!(senderKey.isValid())) return SodiumSecureMemory{};
      if (!(recipientKey.isValid())) return SodiumSecureMemory{};

      // we need at least one message byte, so just the MAC is
      // not sufficient
      if (cipher.getSize() <= crypto_box_MACBYTES) return SodiumSecureMemory{};

      // allocate space for the result
      SodiumSecureMemory msg{cipher.getSize() - crypto_box_MACBYTES, clearTextProtection};

      // decrypt
      int isOkay = sodium.crypto_box_open_easy(msg.get_uc(), cipher.get_uc(), cipher.getSize(), nonce.get_uc(),
                                               senderKey.get_uc(), recipientKey.get_uc());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? std::move(msg) : SodiumSecureMemory{};
    }

    //----------------------------------------------------------------------------

    pair<ManagedBuffer, SodiumLib::AsymCrypto_Tag> SodiumLib::crypto_box_detached(const ManagedMemory& msg, const SodiumLib::AsymCrypto_Nonce& nonce,
                                                                      const SodiumLib::AsymCrypto_PublicKey& recipientKey, const SodiumLib::AsymCrypto_SecretKey& senderKey)
    {
      auto errorResult = make_pair(ManagedBuffer{}, AsymCrypto_Tag{});
      if (!(msg.isValid())) return errorResult;
      if (!(nonce.isValid())) return errorResult;
      if (!(recipientKey.isValid())) return errorResult;
      if (!(senderKey.isValid())) return errorResult;

      // allocate space for the cipher and the MAC
      ManagedBuffer cipher{msg.getSize()};
      AsymCrypto_Tag mac;

      // encrypt
      sodium.crypto_box_detached(cipher.get_uc(), mac.get_uc(), msg.get_uc(), msg.getSize(), nonce.get_uc(),
                                 recipientKey.get_uc(), senderKey.get_uc());

      return make_pair(std::move(cipher), std::move(mac));
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::crypto_box_open_detached(const ManagedMemory& cipher, const SodiumLib::AsymCrypto_Tag& mac, const SodiumLib::AsymCrypto_Nonce& nonce,
                                                           const SodiumLib::AsymCrypto_PublicKey& senderKey, const AsymCrypto_SecretKey& recipientKey,
                                                           SodiumSecureMemType clearTextProtection)
    {
      // the cipher, MAC and keys should be valid
      if (!(cipher.isValid())) return SodiumSecureMemory{};
      if (!(mac.isValid())) return SodiumSecureMemory{};
      if (!(nonce.isValid())) return SodiumSecureMemory{};
      if (!(senderKey.isValid())) return SodiumSecureMemory{};
      if (!(recipientKey.isValid())) return SodiumSecureMemory{};

      // allocate space for the result
      SodiumSecureMemory msg{cipher.getSize(), clearTextProtection};

      // decrypt
      int isOkay = sodium.crypto_box_open_detached(msg.get_uc(), cipher.get_uc(), mac.get_uc(), cipher.getSize(),
                                                   nonce.get_uc(), senderKey.get_uc(), recipientKey.get_uc());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? std::move(msg) : SodiumSecureMemory{};
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_box_easy(const string& msg, const SodiumLib::AsymCrypto_Nonce& nonce, const SodiumLib::AsymCrypto_PublicKey& recipientKey,
                                      const SodiumLib::AsymCrypto_SecretKey& senderKey)
    {
      if (msg.empty()) return string{};
      if (!(nonce.isValid())) return string{};
      if (!(recipientKey.isValid())) return string{};
      if (!(senderKey.isValid())) return string{};

      // allocate space for the result
      string cipher;
      cipher.resize(msg.size() + crypto_box_MACBYTES);

      // the actual encryption
      sodium.crypto_box_easy((unsigned char*)cipher.c_str(), (const unsigned char*)msg.c_str(), msg.size(),
                             nonce.get_uc(), recipientKey.get_uc(), senderKey.get_uc());

      return cipher;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_box_open_easy(const string& cipher, const SodiumLib::AsymCrypto_Nonce& nonce, const SodiumLib::AsymCrypto_PublicKey& senderKey,
                                           const AsymCrypto_SecretKey& recipientKey)
    {
      // the cipher and keys should be valid
      if (cipher.size() <= crypto_box_MACBYTES) return string{};
      if (!(nonce.isValid())) return string{};
      if (!(senderKey.isValid())) return string{};
      if (!(recipientKey.isValid())) return string{};

      // allocate space for the result
      string msg;
      msg.resize(cipher.size() - crypto_box_MACBYTES);

      // decrypt
      int isOkay = sodium.crypto_box_open_easy((unsigned char*)msg.c_str(), (const unsigned char*)cipher.c_str(), cipher.size(),
                                               nonce.get_uc(), senderKey.get_uc(), recipientKey.get_uc());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? msg : string{};
    }

    //----------------------------------------------------------------------------

    pair<string, string> SodiumLib::crypto_box_detached(const string& msg, const SodiumLib::AsymCrypto_Nonce& nonce,
                                                        const SodiumLib::AsymCrypto_PublicKey& recipientKey, const SodiumLib::AsymCrypto_SecretKey& senderKey)
    {
      auto errorResult = make_pair(string{}, string{});
      if (msg.empty()) return errorResult;
      if (!(nonce.isValid())) return errorResult;
      if (!(recipientKey.isValid())) return errorResult;
      if (!(senderKey.isValid())) return errorResult;

      // allocate space for the cipher and the MAC
      string cipher;
      cipher.resize(msg.size());
      string mac;
      mac.resize(crypto_box_MACBYTES);

      // encrypt
      sodium.crypto_box_detached((unsigned char*)cipher.c_str(), (unsigned char*)mac.c_str(),
                                 (const unsigned char*)msg.c_str(), msg.size(),
                                 nonce.get_uc(), recipientKey.get_uc(), senderKey.get_uc());

      return make_pair(std::move(cipher), std::move(mac));
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_box_open_detached(const string& cipher, const string& mac, const SodiumLib::AsymCrypto_Nonce& nonce,
                                               const SodiumLib::AsymCrypto_PublicKey& senderKey, const AsymCrypto_SecretKey& recipientKey)
    {
      // the cipher, MAC and keys should be valid
      if (cipher.empty()) return string{};
      if (mac.size() != crypto_box_MACBYTES) return string{};
      if (!(nonce.isValid())) return string{};
      if (!(senderKey.isValid())) return string{};
      if (!(recipientKey.isValid())) return string{};

      // allocate space for the result
      string msg;
      msg.resize(cipher.size());

      // decrypt
      int isOkay = sodium.crypto_box_open_detached((unsigned char*)msg.c_str(), (const unsigned char*)cipher.c_str(),
                                                   (const unsigned char*)mac.c_str(), cipher.size(),
                                                   nonce.get_uc(), senderKey.get_uc(), recipientKey.get_uc());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? msg : string{};
    }

    //----------------------------------------------------------------------------

    void SodiumLib::genAsymSignKeyPair(SodiumLib::AsymSign_PublicKey& pk_out, SodiumLib::AsymSign_SecretKey& sk_out)
    {
      sodium.crypto_sign_keypair(pk_out.get_uc(), sk_out.get_uc());
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::genAsymSignKeyPairSeeded(const SodiumLib::AsymSign_KeySeed& seed, SodiumLib::AsymSign_PublicKey& pk_out, SodiumLib::AsymSign_SecretKey& sk_out)
    {
      if (!(seed.isValid())) return false;

      sodium.crypto_sign_seed_keypair(pk_out.get_uc(), sk_out.get_uc(), seed.get_uc());
      return true;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::genPublicSignKeyFromSecretKey(const SodiumLib::AsymSign_SecretKey& sk, SodiumLib::AsymSign_PublicKey& pk_out)
    {
      if (!(sk.isValid())) return false;

      sodium.crypto_sign_ed25519_sk_to_pk(pk_out.get_uc(), sk.get_uc());
      return true;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::genSignKeySeedFromSecretKey(const SodiumLib::AsymSign_SecretKey& sk, SodiumLib::AsymSign_KeySeed& seed_out)
    {
      if (!(sk.isValid())) return false;

      sodium.crypto_sign_ed25519_sk_to_seed(seed_out.get_uc(), sk.get_uc());
      return true;
    }

    //----------------------------------------------------------------------------

    ManagedBuffer SodiumLib::crypto_sign(const ManagedMemory& msg, const SodiumLib::AsymSign_SecretKey& sk)
    {
      // check input validity
      if (!(msg.isValid())) return ManagedBuffer{};
      if (!(sk.isValid())) return ManagedBuffer{};

      // allocate space for the result
      ManagedBuffer signedMsg{crypto_sign_BYTES + msg.getSize()};

      // sign
      sodium.crypto_sign(signedMsg.get_uc(), nullptr, msg.get_uc(), msg.getSize(), sk.get_uc());

      return signedMsg;
    }

    //----------------------------------------------------------------------------

    ManagedBuffer SodiumLib::crypto_sign_open(const ManagedMemory& signedMsg, const SodiumLib::AsymSign_PublicKey& pk)
    {
      // check input validity
      if (!(signedMsg.isValid())) return ManagedBuffer{};
      if (!(pk.isValid())) return ManagedBuffer{};
      if (signedMsg.getSize() <= crypto_sign_BYTES) return ManagedBuffer{};

      // allocate space for the result
      ManagedBuffer msg{signedMsg.getSize() - crypto_sign_BYTES};

      // check signature and remove it from the signed message
      int rc = sodium.crypto_sign_open(msg.get_uc(), nullptr, signedMsg.get_uc(), signedMsg.getSize(), pk.get_uc());

      return (rc == 0) ? std::move(msg) : ManagedBuffer{};
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::crypto_sign_detached(const ManagedMemory& msg, const SodiumLib::AsymSign_SecretKey& sk, SodiumLib::AsymSign_Signature& sig_out)
    {
      // check input validity
      if (!(msg.isValid())) return false;
      if (!(sk.isValid())) return false;

      // sign
      sodium.crypto_sign_detached(sig_out.get_uc(), nullptr, msg.get_uc(), msg.getSize(), sk.get_uc());

      return true;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::crypto_sign_verify_detached(const ManagedMemory& msg, const AsymSign_Signature& sig, const SodiumLib::AsymSign_PublicKey& pk)
    {
      // check input validity
      if (!(msg.isValid())) return false;
      if (!(sig.isValid())) return false;
      if (!(pk.isValid())) return false;

      // check signature
      int rc = sodium.crypto_sign_verify_detached(sig.get_uc(), msg.get_uc(), msg.getSize(), pk.get_uc());

      return (rc == 0);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_sign(const string& msg, const SodiumLib::AsymSign_SecretKey& sk)
    {
      // check input validity
      if (msg.empty()) return string{};
      if (!(sk.isValid())) return string{};

      // allocate space for the result
      string signedMsg;
      signedMsg.resize(crypto_sign_BYTES + msg.size());

      // sign
      sodium.crypto_sign((unsigned char*)signedMsg.c_str(), nullptr, (const unsigned char*)msg.c_str(), msg.size(), sk.get_uc());

      return signedMsg;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_sign_open(const string& signedMsg, const SodiumLib::AsymSign_PublicKey& pk)
    {
      // check input validity
      if (signedMsg.size() <= crypto_sign_BYTES) return string{};
      if (!(pk.isValid())) return string{};

      // allocate space for the result
      string msg;
      msg.resize(signedMsg.size() - crypto_sign_BYTES);

      // check signature and remove it from the signed message
      int rc = sodium.crypto_sign_open((unsigned char*)msg.c_str(), nullptr, (const unsigned char*)signedMsg.c_str(), signedMsg.size(), pk.get_uc());

      return (rc == 0) ? msg : string{};
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_sign_detached(const string& msg, const SodiumLib::AsymSign_SecretKey& sk)
    {
      // check input validity
      if (msg.empty()) return string{};
      if (!(sk.isValid())) return string{};

      // allocate space for the result
      string sig;
      sig.resize(crypto_sign_BYTES);

      // sign
      sodium.crypto_sign_detached((unsigned char*)sig.c_str(), nullptr, (const unsigned char*)msg.c_str(), msg.size(), sk.get_uc());

      return sig;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::crypto_sign_verify_detached(const string& msg, const string& sig, const SodiumLib::AsymSign_PublicKey& pk)
    {
      // check input validity
      if (msg.empty()) return false;
      if (!(pk.isValid())) return false;
      if (sig.size() != crypto_sign_BYTES) return false;

      // check signature
      int rc = sodium.crypto_sign_verify_detached((const unsigned char*)sig.c_str(), (const unsigned char*)msg.c_str(), msg.size(), pk.get_uc());

      return (rc == 0);
    }


    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    SodiumSecretBox::SodiumSecretBox(const SodiumSecretBox::KeyType& _key, const SodiumSecretBox::NonceType& _nonce, bool autoIncNonce)
      :NonceBox<SodiumKey<SodiumKeyType::Public, crypto_secretbox_NONCEBYTES>>{_nonce, autoIncNonce}
    {
      key = _key.copy();

      if (!(key.setAccess(SodiumSecureMemAccess::NoAccess)))
      {
        throw SodiumMemoryManagementException{"ctor SecretBox, could not guard private key"};
      }
    }

    //----------------------------------------------------------------------------

    ManagedBuffer SodiumSecretBox::encryptCombined(const ManagedMemory& msg)
    {
      setKeyLockState(false);
      auto result = lib->crypto_secretbox_easy(msg, nextNonce, key);
      setKeyLockState(true);

      if (autoIncrementNonce) incrementNonce();

      return result;
    }

    //----------------------------------------------------------------------------

    pair<ManagedBuffer, ManagedBuffer> SodiumSecretBox::encryptDetached(const ManagedMemory& msg)
    {
      setKeyLockState(false);
      auto result = lib->crypto_secretbox_detached(msg, nextNonce, key);
      setKeyLockState(true);

      if (autoIncrementNonce) incrementNonce();

      return result;
    }

    //----------------------------------------------------------------------------

    string SodiumSecretBox::encryptCombined(const string& msg)
    {
      setKeyLockState(false);
      string result = lib->crypto_secretbox_easy(msg, nextNonce, key);
      setKeyLockState(true);

      if (autoIncrementNonce) incrementNonce();

      return result;
    }

    //----------------------------------------------------------------------------

    pair<string, string> SodiumSecretBox::encryptDetached(const string& msg)
    {
      setKeyLockState(false);
      auto result = lib->crypto_secretbox_detached(msg, nextNonce, key);
      setKeyLockState(true);

      if (autoIncrementNonce) incrementNonce();

      return make_pair(result.first, result.second);
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumSecretBox::decryptCombined(const ManagedMemory& cipher, SodiumSecureMemType clearTextProtection)
    {
      setKeyLockState(false);
      SodiumSecureMemory result = lib->crypto_secretbox_open_easy(cipher, nextNonce, key, clearTextProtection);
      setKeyLockState(true);

      if (autoIncrementNonce) incrementNonce();

      return result;
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumSecretBox::decryptDetached(const ManagedMemory& cipher, const ManagedMemory& mac, SodiumSecureMemType clearTextProtection)
    {
      setKeyLockState(false);
      SodiumSecureMemory result = lib->crypto_secretbox_open_detached(cipher, mac, nextNonce, key, clearTextProtection);
      setKeyLockState(true);

      if (autoIncrementNonce) incrementNonce();

      return result;
    }

    //----------------------------------------------------------------------------

    string SodiumSecretBox::decryptCombined(const string& cipher)
    {
      setKeyLockState(false);
      string msg = lib->crypto_secretbox_open_easy(cipher, nextNonce, key);
      setKeyLockState(true);

      if (autoIncrementNonce) incrementNonce();

      return msg;
    }

    //----------------------------------------------------------------------------

    string SodiumSecretBox::decryptDetached(const string& cipher, const string& mac)
    {
      setKeyLockState(false);
      string result = lib->crypto_secretbox_open_detached(cipher, mac, nextNonce, key);
      setKeyLockState(true);

      if (autoIncrementNonce) incrementNonce();

      return result;
    }

    //----------------------------------------------------------------------------

    void SodiumSecretBox::setKeyLockState(bool setGuard)
    {
      SodiumSecureMemAccess newState = setGuard ? SodiumSecureMemAccess::NoAccess : SodiumSecureMemAccess::RO;

      if (!(key.setAccess(newState)))
      {
        throw SodiumMemoryManagementException{"SecretBox, could not guard / unlock secret key"};
      }
    }

    //----------------------------------------------------------------------------


  }
}
