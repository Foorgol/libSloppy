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

    SodiumSecureMemory SodiumSecureMemory::asCopy(SodiumSecureMemory& src)
    {
      // allocate the same amount and type of memory
      //
      // this might throw exceptions if we're running out of memory
      SodiumSecureMemory cpy{src.getSize(), src.getType()};

      // if we currently can't read the source, un-protect it first
      SodiumSecureMemAccess oldProtection = src.getProtection();
      if (!(src.canRead()))
      {
        bool isOkay = src.setAccess(SodiumSecureMemAccess::RO);
        if (!isOkay)
        {
          throw SodiumMemoryManagementException{"asCopy() for an existing SodiumSecureMemory"};
        }
      }

      // do the copy itself
      memcpy(cpy.get(), src.get(), src.getSize());

      // re-lock the source
      if (src.getType() == SodiumSecureMemType::Guarded)
      {
        bool isOkay = src.setAccess(oldProtection);
        if (!isOkay)
        {
          throw SodiumMemoryManagementException{"re-locking the source in asCopy() for an existing SodiumSecureMemory"};
        }

        // apply the same protection to the copy
        isOkay = cpy.setAccess(oldProtection);
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

    ManagedBuffer SodiumLib::crypto_secretbox_easy(const ManagedMemory& msg, const ManagedMemory& nonce, const SodiumSecureMemory& key)
    {
      // the message should be valid
      if (!(msg.isValid()))  return ManagedBuffer{};

      // make sure that nonce and key have the correct length
      if (nonce.getSize() != crypto_secretbox_NONCEBYTES) return ManagedBuffer{};
      if (key.getSize() != crypto_secretbox_KEYBYTES)  return ManagedBuffer{};

      // allocate space for the result
      ManagedBuffer cipher{crypto_secretbox_MACBYTES + msg.getSize()};

      // do the actual encryption
      sodium.crypto_secretbox_easy(cipher.get_uc(), msg.get_uc(), msg.getSize(), nonce.get_uc(), key.get_uc());

      // return the result
      return cipher;
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::crypto_secretbox_open_easy(const ManagedMemory& cipher, const ManagedMemory& nonce, const ManagedMemory& key, SodiumSecureMemType clearTextProtection)
    {
      // the cipher should be valid
      if (!(cipher.isValid()))  return SodiumSecureMemory{};

      // we need at least one message byte, so just the MAC is
      // not sufficient
      if (cipher.getSize() <= crypto_secretbox_MACBYTES) return SodiumSecureMemory{};

      // make sure that nonce and key have the correct length
      if (nonce.getSize() != crypto_secretbox_NONCEBYTES) return SodiumSecureMemory{};
      if (key.getSize() != crypto_secretbox_KEYBYTES)  return SodiumSecureMemory{};

      // allocate space for the result
      SodiumSecureMemory msg{cipher.getSize() - crypto_secretbox_MACBYTES, clearTextProtection};

      // decrypt
      int isOkay = sodium.crypto_secretbox_open_easy(msg.get_uc(), cipher.get_uc(), cipher.getSize(), nonce.get_uc(), key.get_uc());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? std::move(msg) : SodiumSecureMemory{};
    }

    //----------------------------------------------------------------------------

    pair<ManagedBuffer, ManagedBuffer> SodiumLib::crypto_secretbox_detached(const ManagedMemory& msg, const ManagedMemory& nonce, const SodiumSecureMemory& key)
    {
      pair<ManagedBuffer, ManagedBuffer> errorResult = make_pair(ManagedBuffer{}, ManagedBuffer{});

      // the message should be valid
      if (!(msg.isValid()))  return errorResult;

      // make sure that nonce and key have the correct length
      if (nonce.getSize() != crypto_secretbox_NONCEBYTES) return errorResult;
      if (key.getSize() != crypto_secretbox_KEYBYTES)  return errorResult;

      // allocate space for the result
      ManagedBuffer cipher{msg.getSize()};
      ManagedBuffer mac{crypto_secretbox_MACBYTES};

      // do the actual encryption
      sodium.crypto_secretbox_detached(cipher.get_uc(), mac.get_uc(), msg.get_uc(), msg.getSize(), nonce.get_uc(), key.get_uc());

      // return the result
      return make_pair(std::move(cipher), std::move(mac));
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::crypto_secretbox_open_detached(const ManagedMemory& cipher, const ManagedMemory& mac, const ManagedMemory& nonce, const ManagedMemory& key, SodiumSecureMemType clearTextProtection)
    {
      // the cipher should be valid
      if (!(cipher.isValid()))  return SodiumSecureMemory{};

      // the MAC size should fit
      if (mac.getSize() != crypto_secretbox_MACBYTES) return SodiumSecureMemory{};

      // make sure that nonce and key have the correct length
      if (nonce.getSize() != crypto_secretbox_NONCEBYTES) return SodiumSecureMemory{};
      if (key.getSize() != crypto_secretbox_KEYBYTES)  return SodiumSecureMemory{};

      // allocate space for the result
      SodiumSecureMemory msg{cipher.getSize(), clearTextProtection};

      // decrypt
      int isOkay = sodium.crypto_secretbox_open_detached(msg.get_uc(), cipher.get_uc(), mac.get_uc(), cipher.getSize(), nonce.get_uc(), key.get_uc());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? std::move(msg) : SodiumSecureMemory{};
    }


  }
}
