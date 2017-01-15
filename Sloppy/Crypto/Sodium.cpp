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
          (sodium.crypto_secretbox_open_detached == nullptr)
          //(sodium. == nullptr) ||
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
      if (!(msg.isValid()))  return ManagedBuffer{};

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
      if (!(cipher.isValid()))  return SodiumSecureMemory{};

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
      if (!(msg.isValid()))  return errorResult;

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
      if (!(cipher.isValid()))  return SodiumSecureMemory{};

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
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    SodiumSecretBox::SodiumSecretBox(const SodiumSecretBox::KeyType& _key, const SodiumSecretBox::NonceType& _nonce, bool autoIncNonce)
      :nonceIncrementCount{0}, autoIncrementNonce{autoIncNonce}, lib{SodiumLib::getInstance()}
    {
      if (lib == nullptr)
      {
        throw SodiumNotAvailableException{};
      }

      key = _key.copy();
      initialNonce = _nonce.copy();
      nextNonce = _nonce.copy();
      lastNonce = _nonce.copy();

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

    void SodiumSecretBox::incrementNonce()
    {
      if (nonceIncrementCount > 0) lib->increment(lastNonce);

      lib->increment(nextNonce);

      ++nonceIncrementCount;
    }

    //----------------------------------------------------------------------------

    void SodiumSecretBox::resetNonce()
    {
      nextNonce = initialNonce.copy();
      lastNonce = initialNonce.copy();
      nonceIncrementCount = 0;
    }

    //----------------------------------------------------------------------------

    void SodiumSecretBox::setNonce(const SodiumSecretBox::NonceType& n)
    {
      initialNonce = n.copy();
      resetNonce();
    }

    //----------------------------------------------------------------------------


  }
}
