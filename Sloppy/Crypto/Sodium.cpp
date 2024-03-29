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

#include <dlfcn.h>
#include <iostream>
#include <malloc.h>
#include <cstring>

#include <sodium.h>

#include "Sodium.h"
#include "../Net/Net.h"
#include "Crypto.h"

using namespace std;

namespace Sloppy
{
  namespace Crypto
  {
    // allocate memory for the static instance pointer
    unique_ptr<SodiumLib> SodiumLib::inst;

    //----------------------------------------------------------------------------

    SodiumSecureMemory::SodiumSecureMemory(size_t _len, SodiumSecureMemType t)
      :lib{SodiumLib::getInstance()}, rawPtr{nullptr}, nBytes{_len}, type{t},
        curProtection{SodiumSecureMemAccess::RW}
    {
      if (lib == nullptr)
      {
        throw SodiumNotAvailableException{};
      }

      // allocate the right type of memory
      if ((type == SodiumSecureMemType::Normal) || (type == SodiumSecureMemType::Locked))
      {
        rawPtr = malloc(nBytes);
      }
      if (type == SodiumSecureMemType::Guarded)
      {
        rawPtr = lib->malloc(nBytes);
      }

      // was the allocation successful?
      if (rawPtr == nullptr)
      {
        throw SodiumOutOfMemoryException{"ctor SodiumSecureMemory"};
      }

      // lock, if necessary
      if (type == SodiumSecureMemType::Locked)
      {
        int lockResult = lib->mlock(toNotOwningArray());
        if (lockResult < 0)
        {
          free(rawPtr);
          throw SodiumOutOfMemoryException("ctor SodiumSecureMemory, could not lock memory");
        }
      }
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory::SodiumSecureMemory(const string& src, SodiumSecureMemType t)
      :SodiumSecureMemory{MemView{src}, t} {}

    //----------------------------------------------------------------------------

    SodiumSecureMemory::SodiumSecureMemory(const MemView& src, SodiumSecureMemType t)
      :SodiumSecureMemory{src.size(), t}
    {
      if (src.empty())
      {
        throw std::invalid_argument("SodiumSecureMemory ctor: called with empty initialization data!");
      }

      memcpy(rawPtr, src.to_voidPtr(), nBytes);
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory::SodiumSecureMemory(SodiumSecureMemory&& other)
      :SodiumSecureMemory{}
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
      nBytes = other.nBytes;
      type = other.type;
      lib = other.lib;
      curProtection = other.curProtection;

      // invalidate other's ressources
      other.rawPtr = nullptr;
      other.nBytes = 0;
      other.lib = nullptr;

      return *this;
    }

    //----------------------------------------------------------------------------

    void SodiumSecureMemory::releaseMemory()
    {
      // there's nothing to do for us if don't manage any real memory
      if (rawPtr == nullptr) return;

      // zero-out normal memory, just to be sure
      if (type == SodiumSecureMemType::Normal)
      {
        lib->memzero(toNotOwningArray());
      }

      // remove all access locks
      if (type == SodiumSecureMemType::Locked)
      {
        lib->munlock(toNotOwningArray());
      }

      // release normal memory using standard free()
      if ((type == SodiumSecureMemType::Normal) || (type == SodiumSecureMemType::Locked))
      {
        free(rawPtr);
        rawPtr = nullptr;
      }

      // release guarded memory using sodium's free() function
      if (type == SodiumSecureMemType::Guarded)
      {
        lib->free(rawPtr);
        rawPtr = nullptr;
      }

      // At this point, all memory should be released.
      // If, for some reason, we didn't manage to free all memory,
      // throw an exception
      if (rawPtr != nullptr)
      {
        throw std::runtime_error("SodiumSecureMem: could not release all memory!");
      }

      nBytes = 0;
    }

    //----------------------------------------------------------------------------

    MemView SodiumSecureMemory::toMemView() const
    {
      return MemView(reinterpret_cast<const char *>(rawPtr), nBytes);
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

    bool SodiumSecureMemory::operator ==(const SodiumSecureMemory& other) const
    {
      if ((this->rawPtr == other.rawPtr) && (this->nBytes == other.nBytes)) return true;   // identity

      // we actually have to compare memory contents
      return lib->memcmp(this->toMemView(), other.toMemView());
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory::SodiumSecureMemory(const SodiumSecureMemory& other)
      :SodiumSecureMemory(other.nBytes, other.type)
    {
      if (!(other.canRead()))
      {
        throw SodiumMemoryGuardException{"creating deep SodiumSecureMemory copy"};
      }

      SodiumSecureMemAccess oldProtection = other.getProtection();

      // do the copy itself
      memcpy(rawPtr, other.rawPtr, nBytes);

      // apply the same protection as for the source
      if ((other.getType() == SodiumSecureMemType::Guarded) && (oldProtection != SodiumSecureMemAccess::RW))
      {
        bool isOkay = setAccess(oldProtection);
        if (!isOkay)
        {
          throw SodiumMemoryManagementException{"protecting the copy in the copy ctor of SodiumSecureMemory"};
        }
      }
    }

    //----------------------------------------------------------------------------



    //----------------------------------------------------------------------------

    SodiumLib* SodiumLib::getInstance()
    {
      if (inst == nullptr)
      {
        // clear all old dynamic loader errors
        dlerror();

        // try to load the library; use different names for
        // Windows and Linux
        void *handle = nullptr;
        for (const string libName : {"libsodium.so", "libsodium-23.dll"})
        {
          handle = dlopen(libName.c_str(), RTLD_LAZY);
          if (handle != nullptr) break;
        }

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
      *(reinterpret_cast<void**>(&(sodium.init))) = dlsym(libHandle, "sodium_init");
      *(reinterpret_cast<void**>(&(sodium.bin2hex))) = dlsym(libHandle, "sodium_bin2hex");
      *(reinterpret_cast<void**>(&(sodium.hex2bin))) = dlsym(libHandle, "sodium_hex2bin");
      *(reinterpret_cast<void**>(&(sodium.bin2base64))) = dlsym(libHandle, "sodium_bin2base64");
      *(reinterpret_cast<void**>(&(sodium.base642bin))) = dlsym(libHandle, "sodium_base642bin");
      *(reinterpret_cast<void**>(&(sodium.memcmp))) = dlsym(libHandle, "sodium_memcmp");
      *(reinterpret_cast<void**>(&(sodium.isZero))) = dlsym(libHandle, "sodium_is_zero");
      *(reinterpret_cast<void**>(&(sodium.increment))) = dlsym(libHandle, "sodium_increment");
      *(reinterpret_cast<void**>(&(sodium.add))) = dlsym(libHandle, "sodium_add");
      *(reinterpret_cast<void**>(&(sodium.memzero))) = dlsym(libHandle, "sodium_memzero");
      *(reinterpret_cast<void**>(&(sodium.mlock))) = dlsym(libHandle, "sodium_mlock");
      *(reinterpret_cast<void**>(&(sodium.munlock))) = dlsym(libHandle, "sodium_munlock");
      *(reinterpret_cast<void**>(&(sodium.malloc))) = dlsym(libHandle, "sodium_malloc");
      *(reinterpret_cast<void**>(&(sodium.allocarray))) = dlsym(libHandle, "sodium_allocarray");
      *(reinterpret_cast<void**>(&(sodium.free))) = dlsym(libHandle, "sodium_free");
      *(reinterpret_cast<void**>(&(sodium.mprotect_noaccess))) = dlsym(libHandle, "sodium_mprotect_noaccess");
      *(reinterpret_cast<void**>(&(sodium.mprotect_readonly))) = dlsym(libHandle, "sodium_mprotect_readonly");
      *(reinterpret_cast<void**>(&(sodium.mprotect_readwrite))) = dlsym(libHandle, "sodium_mprotect_readwrite");
      *(reinterpret_cast<void**>(&(sodium.randombytes_random))) = dlsym(libHandle, "randombytes_random");
      *(reinterpret_cast<void**>(&(sodium.randombytes_uniform))) = dlsym(libHandle, "randombytes_uniform");
      *(reinterpret_cast<void**>(&(sodium.randombytes_buf))) = dlsym(libHandle, "randombytes_buf");
      *(reinterpret_cast<void**>(&(sodium.crypto_secretbox_easy))) = dlsym(libHandle, "crypto_secretbox_easy");
      *(reinterpret_cast<void**>(&(sodium.crypto_secretbox_open_easy))) = dlsym(libHandle, "crypto_secretbox_open_easy");
      *(reinterpret_cast<void**>(&(sodium.crypto_secretbox_detached))) = dlsym(libHandle, "crypto_secretbox_detached");
      *(reinterpret_cast<void**>(&(sodium.crypto_secretbox_open_detached))) = dlsym(libHandle, "crypto_secretbox_open_detached");
      *(reinterpret_cast<void**>(&(sodium.crypto_auth))) = dlsym(libHandle, "crypto_auth");
      *(reinterpret_cast<void**>(&(sodium.crypto_auth_verify))) = dlsym(libHandle, "crypto_auth_verify");
      *(reinterpret_cast<void**>(&(sodium.crypto_aead_xchacha20poly1305_ietf_encrypt))) = dlsym(libHandle, "crypto_aead_xchacha20poly1305_ietf_encrypt");
      *(reinterpret_cast<void**>(&(sodium.crypto_aead_xchacha20poly1305_ietf_decrypt))) = dlsym(libHandle, "crypto_aead_xchacha20poly1305_ietf_decrypt");
      *(reinterpret_cast<void**>(&(sodium.crypto_aead_xchacha20poly1305_ietf_encrypt_detached))) = dlsym(libHandle, "crypto_aead_xchacha20poly1305_ietf_encrypt_detached");
      *(reinterpret_cast<void**>(&(sodium.crypto_aead_xchacha20poly1305_ietf_decrypt_detached))) = dlsym(libHandle, "crypto_aead_xchacha20poly1305_ietf_decrypt_detached");
      *(reinterpret_cast<void**>(&(sodium.crypto_aead_aes256gcm_is_available))) = dlsym(libHandle, "crypto_aead_aes256gcm_is_available");
      *(reinterpret_cast<void**>(&(sodium.crypto_aead_aes256gcm_encrypt))) = dlsym(libHandle, "crypto_aead_aes256gcm_encrypt");
      *(reinterpret_cast<void**>(&(sodium.crypto_aead_aes256gcm_decrypt))) = dlsym(libHandle, "crypto_aead_aes256gcm_decrypt");
      *(reinterpret_cast<void**>(&(sodium.crypto_aead_aes256gcm_encrypt_detached))) = dlsym(libHandle, "crypto_aead_aes256gcm_encrypt_detached");
      *(reinterpret_cast<void**>(&(sodium.crypto_aead_aes256gcm_decrypt_detached))) = dlsym(libHandle, "crypto_aead_aes256gcm_decrypt_detached");
      *(reinterpret_cast<void**>(&(sodium.crypto_box_keypair))) = dlsym(libHandle, "crypto_box_keypair");
      *(reinterpret_cast<void**>(&(sodium.crypto_box_seed_keypair))) = dlsym(libHandle, "crypto_box_seed_keypair");
      *(reinterpret_cast<void**>(&(sodium.crypto_scalarmult_base))) = dlsym(libHandle, "crypto_scalarmult_base");
      *(reinterpret_cast<void**>(&(sodium.crypto_box_easy))) = dlsym(libHandle, "crypto_box_easy");
      *(reinterpret_cast<void**>(&(sodium.crypto_box_open_easy))) = dlsym(libHandle, "crypto_box_open_easy");
      *(reinterpret_cast<void**>(&(sodium.crypto_box_detached))) = dlsym(libHandle, "crypto_box_detached");
      *(reinterpret_cast<void**>(&(sodium.crypto_box_open_detached))) = dlsym(libHandle, "crypto_box_open_detached");
      *(reinterpret_cast<void**>(&(sodium.crypto_sign_keypair))) = dlsym(libHandle, "crypto_sign_keypair");
      *(reinterpret_cast<void**>(&(sodium.crypto_sign_seed_keypair))) = dlsym(libHandle, "crypto_sign_seed_keypair");
      *(reinterpret_cast<void**>(&(sodium.crypto_sign))) = dlsym(libHandle, "crypto_sign");
      *(reinterpret_cast<void**>(&(sodium.crypto_sign_open))) = dlsym(libHandle, "crypto_sign_open");
      *(reinterpret_cast<void**>(&(sodium.crypto_sign_detached))) = dlsym(libHandle, "crypto_sign_detached");
      *(reinterpret_cast<void**>(&(sodium.crypto_sign_verify_detached))) = dlsym(libHandle, "crypto_sign_verify_detached");
      *(reinterpret_cast<void**>(&(sodium.crypto_sign_ed25519_sk_to_seed))) = dlsym(libHandle, "crypto_sign_ed25519_sk_to_seed");
      *(reinterpret_cast<void**>(&(sodium.crypto_sign_ed25519_sk_to_pk))) = dlsym(libHandle, "crypto_sign_ed25519_sk_to_pk");
      *(reinterpret_cast<void**>(&(sodium.crypto_generichash))) = dlsym(libHandle, "crypto_generichash");
      *(reinterpret_cast<void**>(&(sodium.crypto_generichash_init))) = dlsym(libHandle, "crypto_generichash_init");
      *(reinterpret_cast<void**>(&(sodium.crypto_generichash_update))) = dlsym(libHandle, "crypto_generichash_update");
      *(reinterpret_cast<void**>(&(sodium.crypto_generichash_final))) = dlsym(libHandle, "crypto_generichash_final");
      *(reinterpret_cast<void**>(&(sodium.crypto_generichash_statebytes))) = dlsym(libHandle, "crypto_generichash_statebytes");
      *(reinterpret_cast<void**>(&(sodium.crypto_shorthash))) = dlsym(libHandle, "crypto_shorthash");
      *(reinterpret_cast<void**>(&(sodium.crypto_pwhash))) = dlsym(libHandle, "crypto_pwhash");
      *(reinterpret_cast<void**>(&(sodium.crypto_pwhash_str))) = dlsym(libHandle, "crypto_pwhash_str");
      *(reinterpret_cast<void**>(&(sodium.crypto_pwhash_str_verify))) = dlsym(libHandle, "crypto_pwhash_str_verify");
      *(reinterpret_cast<void**>(&(sodium.crypto_pwhash_scryptsalsa208sha256))) = dlsym(libHandle, "crypto_pwhash_scryptsalsa208sha256");
      *(reinterpret_cast<void**>(&(sodium.crypto_pwhash_scryptsalsa208sha256_str))) = dlsym(libHandle, "crypto_pwhash_scryptsalsa208sha256_str");
      *(reinterpret_cast<void**>(&(sodium.crypto_pwhash_scryptsalsa208sha256_str_verify))) = dlsym(libHandle, "crypto_pwhash_scryptsalsa208sha256_str_verify");
      *(reinterpret_cast<void**>(&(sodium.crypto_scalarmult))) = dlsym(libHandle, "crypto_scalarmult");
      *(reinterpret_cast<void**>(&(sodium.crypto_kx_keypair))) = dlsym(libHandle, "crypto_kx_keypair");
      *(reinterpret_cast<void**>(&(sodium.crypto_kx_seed_keypair))) = dlsym(libHandle, "crypto_kx_seed_keypair");
      *(reinterpret_cast<void**>(&(sodium.crypto_kx_client_session_keys))) = dlsym(libHandle, "crypto_kx_client_session_keys");
      *(reinterpret_cast<void**>(&(sodium.crypto_kx_server_session_keys))) = dlsym(libHandle, "crypto_kx_server_session_keys");

      // make sure we've successfully loaded all symbols
      if ((sodium.init == nullptr) ||
          (sodium.bin2hex == nullptr) ||
          (sodium.hex2bin == nullptr) ||
          (sodium.bin2base64 == nullptr) ||
          (sodium.base642bin == nullptr) ||
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
          (sodium.crypto_aead_xchacha20poly1305_ietf_encrypt == nullptr) ||
          (sodium.crypto_aead_xchacha20poly1305_ietf_decrypt == nullptr) ||
          (sodium.crypto_aead_xchacha20poly1305_ietf_encrypt_detached == nullptr) ||
          (sodium.crypto_aead_xchacha20poly1305_ietf_decrypt_detached == nullptr) ||
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
          (sodium.crypto_sign_ed25519_sk_to_pk == nullptr) ||
          (sodium.crypto_generichash == nullptr) ||
          (sodium.crypto_generichash_init == nullptr) ||
          (sodium.crypto_generichash_update == nullptr) ||
          (sodium.crypto_generichash_final == nullptr) ||
          (sodium.crypto_generichash_statebytes == nullptr) ||
          (sodium.crypto_shorthash == nullptr) ||
          (sodium.crypto_pwhash == nullptr) ||
          (sodium.crypto_pwhash_str == nullptr) ||
          (sodium.crypto_pwhash_str_verify == nullptr) ||
          (sodium.crypto_pwhash_scryptsalsa208sha256 == nullptr) ||
          (sodium.crypto_pwhash_scryptsalsa208sha256_str == nullptr) ||
          (sodium.crypto_pwhash_scryptsalsa208sha256_str_verify == nullptr) ||
          (sodium.crypto_scalarmult == nullptr) ||
          (sodium.crypto_kx_keypair == nullptr) ||
          (sodium.crypto_kx_seed_keypair == nullptr) ||
          (sodium.crypto_kx_client_session_keys == nullptr) ||
          (sodium.crypto_kx_server_session_keys == nullptr)
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

    bool SodiumLib::memcmp(const MemView& m1, const MemView& m2) const
    {
      if (m1.size() != m2.size()) return false;

      return (sodium.memcmp(m1.to_voidPtr(), m2.to_voidPtr(), m1.size()) == 0);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::bin2hex(const string& binData) const
    {
      if (binData.empty()) return string{};

      string result;
      size_t resultSize = binData.size() * 2;
      result.resize(resultSize);  // this actually allocates size+1 bytes, because std::string internally manages a terminating zero

      // tell sodium that it may write size+1 bytes ("size" data bytes plus the terminating zero)
      sodium.bin2hex((char *)result.c_str(), resultSize + 1,
                     reinterpret_cast<const unsigned char*>(binData.c_str()), binData.size());

      return result;
    }

    //----------------------------------------------------------------------------

    MemArray SodiumLib::bin2hex(const MemView& binData) const
    {
      if (binData.empty()) return MemArray{};

      // prepare a result array with 2 x hexDataLength + 1 bytes for the terminating zero
      MemArray result{binData.size() * 2 + 1};

      sodium.bin2hex(result.to_charPtr(), result.size(), binData.to_ucPtr(), binData.size());

      return result;
    }

    //----------------------------------------------------------------------------

    MemArray SodiumLib::hex2bin(const MemView& hex, const string& ignore) const
    {
      if (hex.empty()) return MemArray{};

      // conservative assumption: we can't have more than output bytes
      // than half the size of the input. Rational: every output byte
      // requires two input bytes (e.g., "0A" ==> 10). If we have additional
      // ignore characters in the input data, our output size calculation becomes
      // even more conservative
      MemArray binData{hex.size() / 2};  // no need to accound for rounding: any "odd" character must an ignore character because a single byte always needs two chars

      // do we have a list of ignore chars?
      const char * ignorePtr = (ignore.empty()) ? nullptr : ignore.c_str();

      // the conversion itself
      size_t actualBinLen = 0;
      int rc = sodium.hex2bin(binData.to_ucPtr(), binData.size(), hex.to_charPtr(), hex.size(),
                       ignorePtr, &actualBinLen, nullptr);

      // conversion errors occurred
      if (rc != 0)
      {
        throw SodiumConversionError{"hex2bin conversion (MemView --> MemArray)"};
      }

      // resize the output buffer to the actual size
      //
      // it is okay to call this without prior checks,
      // because resize() returns without allocation or copying
      // if old and new size match.
      binData.resize(actualBinLen);

      return binData;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::hex2bin(const string& hex, const string& ignore) const
    {
      if (hex.empty()) return string{};

      // conservative assumption: we can't have more than output bytes
      // than half the size of the input. Rational: every output byte
      // requires two input bytes (e.g., "0A" ==> 10). If we have additional
      // ignore characters in the input data, our output size calculation becomes
      // even more conservative
      string binData;
      binData.resize(hex.size() / 2);  // no need to accound for rounding: any "odd" character must an ignore character because a single byte always needs two chars

      // do we have a list of ignore chars?
      const char * ignorePtr = (ignore.empty()) ? nullptr : ignore.c_str();

      // the conversion itself
      size_t actualBinLen = 0;
      int rc = sodium.hex2bin((unsigned char *)binData.c_str(), binData.capacity(), hex.c_str(), hex.size(),
                       ignorePtr, &actualBinLen, nullptr);

      // conversion errors occurred
      if (rc != 0)
      {
        throw SodiumConversionError{"hex2bin conversion (string --> string)"};
      }

      // resize the output buffer to the actual size
      binData.resize(actualBinLen);
      binData.shrink_to_fit();

      return binData;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::bin2Base64(const string& bin, SodiumBase64Enconding enc) const
    {
      if (bin.empty()) return string{};

      // determine the encoding variant
      int variant = SodiumBase64Enconding2int(enc);

      // calculate the number of output characters including a trailing zero-byte
      size_t outLen = sodium_base64_ENCODED_LEN(bin.size(), variant);

      // allocate sufficient memory including a trailing zero-byte
      string out;
      out.resize(outLen);

      // do the conversion
      char * rc = sodium.bin2base64((char *) out.c_str(), outLen,
                        reinterpret_cast<const unsigned char*>(bin.c_str()), bin.size(),
                        variant);

      if (rc == nullptr)
      {
        throw SodiumConversionError{"bin2base64 encoding (string --> string)"};
      }

      return out.substr(0, out.size() - 1);   // cut away the trailing zero provided by libSodium
    }

    //----------------------------------------------------------------------------

    MemArray SodiumLib::bin2Base64(const MemView& bin, SodiumBase64Enconding enc) const
    {
      if (bin.empty()) return MemArray{};

      // determine the encoding variant
      int variant = SodiumBase64Enconding2int(enc);

      // calculate the number of output characters including a trailing zero-byte
      size_t outLen = sodium_base64_ENCODED_LEN(bin.size(), variant);

      // allocate sufficient memory including a trailing zero-byte
      MemArray out{outLen};

      // do the conversion
      char * rc = sodium.bin2base64(out.to_charPtr(), outLen,
                        bin.to_ucPtr(), bin.size(),
                        variant);

      if (rc == nullptr)
      {
        throw SodiumConversionError{"bin2base64 encoding (MemView --> MemArray)"};
      }

      // cut off the trailing zero provided by libSodium;
      // unfortunately, this requires a memory re-allocation and
      // a deep copy operation...
      out.resize(out.size() - 1);

      return out;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::base642Bin(const string& b64, const string& ignore, SodiumBase64Enconding enc) const
    {
      if (b64.empty()) return string{};

      // determine the encoding variant
      int variant = SodiumBase64Enconding2int(enc);

      // Ultra-conservative assumption: the output size is equal to the input size
      string bin;
      bin.resize(b64.size());

      // do we have a list of ignore chars?
      const char * ignorePtr = (ignore.empty()) ? nullptr : ignore.c_str();

      // the actual conversion
      size_t actualBinLen = 0;
      int rc = sodium.base642bin((unsigned char *)bin.c_str(), b64.size(), b64.c_str(), b64.size(),
                                 ignorePtr, &actualBinLen, nullptr, variant);
      if (rc != 0)
      {
        throw SodiumConversionError{"base642bin decoding (string --> string)"};
      }

      // adjust the output size
      bin.resize(actualBinLen);
      bin.shrink_to_fit();

      return bin;
    }

    //----------------------------------------------------------------------------

    MemArray SodiumLib::base642Bin(const MemView& b64, const string& ignore, SodiumBase64Enconding enc) const
    {
      if (b64.empty()) return MemArray{};

      // determine the encoding variant
      int variant = SodiumBase64Enconding2int(enc);

      // Ultra-conservative assumption: the output size is equal to the input size
      MemArray bin{b64.size()};

      // do we have a list of ignore chars?
      const char * ignorePtr = (ignore.empty()) ? nullptr : ignore.c_str();

      // the actual conversion
      size_t actualBinLen = 0;
      int rc = sodium.base642bin(bin.to_ucPtr(), b64.size(), b64.to_charPtr(), b64.size(),
                                 ignorePtr, &actualBinLen, nullptr, variant);
      if (rc != 0)
      {
        throw SodiumConversionError{"base642bin decoding (MemView --> MemArray)"};
      }

      // adjust the output size
      bin.resize(actualBinLen);

      return bin;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::isZero(const MemView& buf) const
    {
      return (sodium.isZero(buf.to_ucPtr(), buf.size()) == 1);
    }

    //----------------------------------------------------------------------------

    void SodiumLib::increment(const MemArray& buf)
    {
      sodium.increment(buf.to_ucPtr(), buf.size());
    }

    //----------------------------------------------------------------------------

    void SodiumLib::add(const MemArray& a, const MemView& b)
    {
      if (a.size() != b.size())
      {
        throw SodiumInvalidKeysize{"the size of two large numbers for adding did not match"};
      }

      sodium.add(a.to_ucPtr(), b.to_ucPtr(), b.size());
    }

    //----------------------------------------------------------------------------

    void SodiumLib::memzero(const MemArray& buf)
    {
      sodium.memzero(buf.to_voidPtr(), buf.size());
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::mlock(const MemArray& buf)
    {
      return (sodium.mlock(buf.to_voidPtr(), buf.size()) != -1);
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::munlock(const MemArray& buf)
    {
      return (sodium.munlock(buf.to_voidPtr(), buf.size()) == 0);
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

    void SodiumLib::randombytes_buf(const MemArray& buf) const
    {
      sodium.randombytes_buf(buf.to_voidPtr(), buf.size());
    }

    //----------------------------------------------------------------------------

    MemArray SodiumLib::secretbox_easy(const MemView& msg, const SecretBoxNonce& nonce, const SecretBoxKey& key)
    {
      // the message and the other parameters should be valid
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_secretbox_easy");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_secretbox_easy");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_secretbox_easy");
      }

      // allocate space for the result
      MemArray cipher{crypto_secretbox_MACBYTES + msg.size()};

      // do the actual encryption
      sodium.crypto_secretbox_easy(cipher.to_ucPtr(), msg.to_ucPtr(), msg.size(), nonce.to_ucPtr_ro(), key.to_ucPtr_ro());

      // return the result
      return cipher;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::secretbox_open_easy__internal(const MemArray& targetBuf, const MemView& cipher, const SodiumLib::SecretBoxNonce& nonce, const SodiumLib::SecretBoxKey& key)
    {
      // the parameters should be valid
      if (cipher.empty() || (cipher.size() <= crypto_secretbox_MACBYTES))
      {
        throw SodiumInvalidCipher("crypto_secretbox_open_easy");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_secretbox_open_easy");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_secretbox_open_easy");
      }
      if (targetBuf.empty() || (targetBuf.size() != (cipher.size() - crypto_secretbox_MACBYTES)))
      {
        throw SodiumInvalidBuffer("crypto_secretbox_open_easy");
      }

      // decrypt
      int isOkay = sodium.crypto_secretbox_open_easy(targetBuf.to_ucPtr(), cipher.to_ucPtr(), cipher.size(), nonce.to_ucPtr_ro(), key.to_ucPtr_ro());

      // return the clear text or an invalid buffer
      return (isOkay == 0);
    }

    //----------------------------------------------------------------------------

    MemArray SodiumLib::secretbox_open_easy(const MemView& cipher, const SodiumLib::SecretBoxNonce& nonce, const SodiumLib::SecretBoxKey& key)
    {
      // we need at least one message byte, so just the MAC is
      // not sufficient
      if (cipher.size() <= crypto_secretbox_MACBYTES)
      {
        throw SodiumInvalidCipher("crypto_secretbox_open_easy");
      }

      // allocate space for the result
      MemArray msg{cipher.size() - crypto_secretbox_MACBYTES};

      // decrypt
      bool isOkay = secretbox_open_easy__internal(msg, cipher, nonce, key);

      // return the clear text or an invalid buffer
      return isOkay ? std::move(msg) : MemArray{};
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::secretbox_open_easy__secure(const MemView& cipher, const SecretBoxNonce& nonce, const SecretBoxKey& key, SodiumSecureMemType clearTextProtection)
    {
      // we need at least one message byte, so just the MAC is
      // not sufficient
      if (cipher.size() <= crypto_secretbox_MACBYTES)
      {
        throw SodiumInvalidCipher("crypto_secretbox_open_easy");
      }

      // allocate space for the result
      SodiumSecureMemory msg{cipher.size() - crypto_secretbox_MACBYTES, clearTextProtection};

      // decrypt
      bool isOkay = secretbox_open_easy__internal(msg.toNotOwningArray(), cipher, nonce, key);

      // return the clear text or an invalid buffer
      return isOkay ? std::move(msg) : SodiumSecureMemory{};
    }

    //----------------------------------------------------------------------------

    pair<MemArray, SodiumLib::SecretBoxMac> SodiumLib::secretbox_detached(const MemView& msg, const SecretBoxNonce& nonce, const SecretBoxKey& key)
    {
      // the message and the other parameters should be valid
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_secretbox_easy");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_secretbox_easy");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_secretbox_easy");
      }

      // allocate space for the result
      MemArray cipher{msg.size()};
      SodiumLib::SecretBoxMac mac;

      // do the actual encryption
      sodium.crypto_secretbox_detached(cipher.to_ucPtr(), mac.to_ucPtr_rw(), msg.to_ucPtr(), msg.size(), nonce.to_ucPtr_ro(), key.to_ucPtr_ro());

      // return the result
      return make_pair(std::move(cipher), std::move(mac));
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::secretbox_open_detached(const MemView& cipher, const SecretBoxMac& mac, const SecretBoxNonce& nonce, const SecretBoxKey& key, SodiumSecureMemType clearTextProtection)
    {
      // the parameters should be valid
      if (cipher.empty())
      {
        throw SodiumInvalidCipher("crypto_secretbox_open_detached");
      }
      if (mac.empty())
      {
        throw SodiumInvalidMac("crypto_secretbox_open_detached");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_secretbox_open_detached");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_secretbox_open_detached");
      }

      // allocate space for the result
      SodiumSecureMemory msg{cipher.size(), clearTextProtection};

      // decrypt
      int isOkay = sodium.crypto_secretbox_open_detached(msg.to_ucPtr_rw(), cipher.to_ucPtr(), mac.to_ucPtr_ro(), cipher.size(), nonce.to_ucPtr_ro(), key.to_ucPtr_ro());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? std::move(msg) : SodiumSecureMemory{};
    }

    //----------------------------------------------------------------------------

    string SodiumLib::secretbox_easy(const string& msg, const SecretBoxNonce& nonce, const SecretBoxKey& key)
    {
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_secretbox_easy");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_secretbox_easy");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_secretbox_easy");
      }

      // allocate space for the result
      string cipher;
      cipher.resize(crypto_secretbox_MACBYTES + msg.size());

      // do the actual encryption
      sodium.crypto_secretbox_easy(
            (unsigned char *)(cipher.c_str()),
            reinterpret_cast<const unsigned char *>(msg.c_str()),
            msg.size(), nonce.to_ucPtr_ro(), key.to_ucPtr_ro());

      // return the result
      return cipher;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::secretbox_open_easy(const string& cipher, const SecretBoxNonce& nonce, const SecretBoxKey& key)
    {
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_secretbox_open_easy");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_secretbox_open_easy");
      }

      // we need at least one message byte, so just the MAC is
      // not sufficient
      if (cipher.empty() || (cipher.size() <= crypto_secretbox_MACBYTES))
      {
        throw SodiumInvalidCipher("crypto_secretbox_open_easy");
      }

      // allocate space for the result
      string msg;
      msg.resize(cipher.size() - crypto_secretbox_MACBYTES);

      // decrypt
      int isOkay = sodium.crypto_secretbox_open_easy((unsigned char *)msg.c_str(),
                                                     reinterpret_cast<const unsigned char *>(cipher.c_str()), cipher.size(),
                                                     nonce.to_ucPtr_ro(), key.to_ucPtr_ro());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? msg : string{};
    }

    //----------------------------------------------------------------------------

    pair<string, SodiumLib::SecretBoxMac> SodiumLib::secretbox_detached(const string& msg, const SecretBoxNonce& nonce, const SecretBoxKey& key)
    {
      // the message and the other parameters should be valid
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_secretbox_easy");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_secretbox_easy");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_secretbox_easy");
      }

      // allocate space for the result
      string cipher;
      cipher.resize(msg.size());
      SecretBoxMac mac;

      // do the actual encryption
      sodium.crypto_secretbox_detached((unsigned char *)cipher.c_str(), mac.to_ucPtr_rw(),
                                       reinterpret_cast<const unsigned char *>(msg.c_str()), msg.size(),
                                       nonce.to_ucPtr_ro(), key.to_ucPtr_ro());

      // return the result
      return make_pair(std::move(cipher), std::move(mac));
    }

    //----------------------------------------------------------------------------

    string SodiumLib::secretbox_open_detached(const string& cipher, const SecretBoxMac& mac, const SecretBoxNonce& nonce, const SecretBoxKey& key)
    {
      // the parameters should be valid
      if (cipher.empty())
      {
        throw SodiumInvalidCipher("crypto_secretbox_open_detached");
      }
      if (mac.empty())
      {
        throw SodiumInvalidMac("crypto_secretbox_open_detached");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_secretbox_open_detached");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_secretbox_open_detached");
      }

      // allocate space for the result
      string msg;
      msg.resize(cipher.size());

      // decrypt
      int isOkay = sodium.crypto_secretbox_open_detached((unsigned char *)msg.c_str(),
                                                         reinterpret_cast<const unsigned char *>(cipher.c_str()),
                                                         mac.to_ucPtr_ro(), cipher.size(),
                                                         nonce.to_ucPtr_ro(), key.to_ucPtr_ro());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? msg  : string{};
    }

    //----------------------------------------------------------------------------

    SodiumLib::AuthTagType SodiumLib::auth(const MemView& msg, const SodiumLib::AuthKeyType& key)
    {
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_auth");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_auth");
      }

      AuthTagType result;
      sodium.crypto_auth(result.to_ucPtr_rw(), msg.to_ucPtr(), msg.size(), key.to_ucPtr_ro());

      return result;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::auth_verify(const MemView& msg, const SodiumLib::AuthTagType& tag, const SodiumLib::AuthKeyType& key)
    {
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_auth_verify");
      }
      if (tag.empty())
      {
        throw SodiumInvalidMac("crypto_auth_verify");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_auth_verify");
      }

      return (sodium.crypto_auth_verify(tag.to_ucPtr_ro(), msg.to_ucPtr(), msg.size(), key.to_ucPtr_ro()) == 0);
    }

    //----------------------------------------------------------------------------

    SodiumLib::AuthTagType SodiumLib::auth(const string& msg, const AuthKeyType& key)
    {
      // Convert the input string to a memory view
      // and call the "master function"
      MemView mv{msg};
      return auth(mv, key);
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::auth_verify(const string& msg, const AuthTagType& tag, const AuthKeyType& key)
    {
      // Convert the input string to a memory view
      // and call the "master function"
      MemView mv{msg};
      return auth_verify(mv, tag, key);
    }

    //----------------------------------------------------------------------------

    MemArray SodiumLib::aead_encrypt(int (*funcPtr)(unsigned char*, unsigned long long*, const unsigned char*, unsigned long long, const unsigned char*,
                                                                unsigned long long, const unsigned char*, const unsigned char*, const unsigned char*), size_t tagSize,
                                                 const MemView& msg, const MemView& nonce, const MemView& key, const MemView& ad)
    {
      if (funcPtr == nullptr)
      {
        throw std::invalid_argument("nullptr for encryption function provided to crypto_aead_encrypt()");
      }
      if (tagSize == 0)
      {
        throw SodiumInvalidMac("crypto_aead_encrypt");
      }
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_aead_encrypt");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_aead_encrypt");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_aead_encrypt");
      }

      // alocate space for the result
      size_t maxCipherLen = msg.size() + tagSize;
      MemArray cipher{maxCipherLen};

      // do we use additional data?
      unsigned const char *adPtr = nullptr;
      size_t adLen = 0;
      if (ad.notEmpty())
      {
        adPtr = ad.to_ucPtr();
        adLen = ad.size();
      }

      // call the encryption function
      unsigned long long actualCipherLen = 0;
      funcPtr(cipher.to_ucPtr(), &actualCipherLen, msg.to_ucPtr(), msg.size(),
              adPtr, adLen, nullptr, nonce.to_ucPtr(), key.to_ucPtr());

      // do we need to shrink the result?
      if (actualCipherLen < maxCipherLen)
      {
        cipher.resize(actualCipherLen);
      }

      return cipher;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::aead_encrypt(int (*funcPtr)(unsigned char*, unsigned long long*, const unsigned char*, unsigned long long, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*, const unsigned char*),
                                          size_t tagSize, const string& msg, const MemView& nonce,
                                          const MemView& key, const string& ad)
    {
      if (funcPtr == nullptr)
      {
        throw std::invalid_argument("nullptr for encryption function provided to crypto_aead_encrypt()");
      }
      if (tagSize == 0)
      {
        throw SodiumInvalidMac("crypto_aead_encrypt");
      }
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_aead_encrypt");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_aead_encrypt");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_aead_encrypt");
      }

      // alocate space for the result
      size_t maxCipherLen = msg.size() + tagSize;
      string cipher;
      cipher.resize(maxCipherLen);

      // do we use additional data?
      unsigned const char *adPtr = nullptr;
      size_t adLen = 0;
      if (!(ad.empty()))
      {
        adPtr = reinterpret_cast<unsigned const char *>(ad.c_str());
        adLen = ad.size();
      }

      // call the encryption function
      unsigned long long actualCipherLen = 0;
      funcPtr((unsigned char *)cipher.c_str(), &actualCipherLen,
              reinterpret_cast<unsigned const char *>(msg.c_str()), msg.size(),
              adPtr, adLen, nullptr, nonce.to_ucPtr(), key.to_ucPtr());

      // do we need to shrink the result?
      if (actualCipherLen < maxCipherLen)
      {
        cipher.resize(actualCipherLen);
      }

      return cipher;
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::aead_decrypt(
        int (*funcPtr)(unsigned char*, unsigned long long*, unsigned char*, const unsigned char*, unsigned long long, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*),
        size_t tagSize, const MemView& cipher, const MemView& nonce,
        const MemView& key, const MemView& ad, SodiumSecureMemType clearTextProtection)
    {
      if (funcPtr == nullptr)
      {
        throw std::invalid_argument("nullptr for encryption function provided to crypto_aead_decrypt()");
      }

      if (cipher.empty())
      {
        throw SodiumInvalidCipher("crypto_aead_decrypt");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_aead_decrypt");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_aead_decrypt");
      }

      // we need to have at least one byte of message data besides the tag
      if (cipher.size() <= tagSize)
      {
        throw SodiumInvalidCipher("crypto_aead_decrypt");
      }

      // calc the maximum message size
      size_t maxMsgLen = cipher.size() - tagSize;
      SodiumSecureMemory msg{maxMsgLen, clearTextProtection};

      // do we use additional data?
      unsigned const char *adPtr = nullptr;
      size_t adLen = 0;
      if (ad.notEmpty())
      {
        adPtr = ad.to_ucPtr();
        adLen = ad.size();
      }

      // call the decryption function
      unsigned long long actualMsgLen = 0;
      int rc = funcPtr(msg.to_ucPtr_rw(), &actualMsgLen, nullptr, cipher.to_ucPtr(), cipher.size(),
                                                  adPtr, adLen, nonce.to_ucPtr(), key.to_ucPtr());

      if (rc != 0) return SodiumSecureMemory{};  // verification failed

      // do we need to shrink the result?
      if (actualMsgLen < maxMsgLen)
      {
        SodiumSecureMemory m{actualMsgLen, clearTextProtection};
        memcpy(m.to_ucPtr_rw(), msg.to_ucPtr_ro(), actualMsgLen);
        return m;
      }

      return msg;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::aead_decrypt(int (*funcPtr)(unsigned char*, unsigned long long*, unsigned char*, const unsigned char*, unsigned long long, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*), size_t tagSize, const string& cipher, const MemView& nonce, const MemView& key, const string& ad)
    {
      if (funcPtr == nullptr)
      {
        throw std::invalid_argument("nullptr for encryption function provided to crypto_aead_decrypt()");
      }

      if (cipher.empty())
      {
        throw SodiumInvalidCipher("crypto_aead_decrypt");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_aead_decrypt");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_aead_decrypt");
      }

      // we need to have at least one byte of message data besides the tag
      if (cipher.size() <= tagSize)
      {
        throw SodiumInvalidCipher("crypto_aead_decrypt");
      }

      // calc the maximum message size
      size_t maxMsgLen = cipher.size() - tagSize;
      string msg;
      msg.resize(maxMsgLen);

      // do we use additional data?
      unsigned const char *adPtr = nullptr;
      size_t adLen = 0;
      if (!(ad.empty()))
      {
        adPtr = reinterpret_cast<unsigned const char *>(ad.c_str());
        adLen = ad.size();
      }

      // call the decryption function
      unsigned long long actualMsgLen = 0;
      int rc = funcPtr((unsigned char *)msg.c_str(), &actualMsgLen, nullptr,
                       reinterpret_cast<const unsigned char *>(cipher.c_str()), cipher.size(),
                       adPtr, adLen, nonce.to_ucPtr(), key.to_ucPtr());

      if (rc != 0) return string{};  // verification failed

      // do we need to shrink the result?
      if (actualMsgLen < maxMsgLen)
      {
        msg.resize(actualMsgLen);
      }

      return msg;
    }


    //----------------------------------------------------------------------------

    pair<unsigned long long, size_t> SodiumLib::pwHashConfigToValues(SodiumLib::PasswdHashStrength strength)
    {
      unsigned long long opslimit;
      size_t memlimit;

      switch (strength)
      {
      case PasswdHashStrength::Interactive:
        opslimit = crypto_pwhash_OPSLIMIT_INTERACTIVE;
        memlimit = crypto_pwhash_MEMLIMIT_INTERACTIVE;
        break;

      case PasswdHashStrength::Moderate:
        opslimit = crypto_pwhash_OPSLIMIT_MODERATE;
        memlimit = crypto_pwhash_MEMLIMIT_MODERATE;
        break;

      default:
        opslimit = crypto_pwhash_OPSLIMIT_SENSITIVE;
        memlimit = crypto_pwhash_MEMLIMIT_SENSITIVE;
      }

      return make_pair(opslimit, memlimit);
    }

    //----------------------------------------------------------------------------

    MemArray SodiumLib::aead_xchacha20poly1305_encrypt(const MemView& msg, const AEAD_XChaCha20Poly1305_NonceType& nonce,
                                                             const AEAD_XChaCha20Poly1305_KeyType& key, const MemView& ad)
    {
      return aead_encrypt(sodium.crypto_aead_xchacha20poly1305_ietf_encrypt,
                                 crypto_aead_xchacha20poly1305_ietf_ABYTES, msg, nonce.toMemView(),
                                 key.toMemView(), ad);
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::aead_xchacha20poly1305_decrypt(const MemView& cipher, const AEAD_XChaCha20Poly1305_NonceType& nonce,
                                                                       const AEAD_XChaCha20Poly1305_KeyType& key, const MemView& ad,
                                                                       SodiumSecureMemType clearTextProtection)
    {
      return aead_decrypt(sodium.crypto_aead_xchacha20poly1305_ietf_decrypt,
                                 crypto_aead_xchacha20poly1305_ietf_ABYTES,
                                 cipher, nonce.toMemView(), key.toMemView(),
                                 ad, clearTextProtection);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::aead_xchacha20poly1305_encrypt(const string& msg,
                                                            const AEAD_XChaCha20Poly1305_NonceType& nonce,
                                                            const AEAD_XChaCha20Poly1305_KeyType& key, const string& ad)
    {
      return aead_encrypt(sodium.crypto_aead_xchacha20poly1305_ietf_encrypt,
                                 crypto_aead_xchacha20poly1305_ietf_ABYTES,
                                 msg, nonce.toMemView(), key.toMemView(),
                                 ad);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::aead_xchacha20poly1305_decrypt(const string& cipher,
                                                            const SodiumLib::AEAD_XChaCha20Poly1305_NonceType& nonce,
                                                            const SodiumLib::AEAD_XChaCha20Poly1305_KeyType& key, const string& ad)
    {
      return aead_decrypt(sodium.crypto_aead_xchacha20poly1305_ietf_decrypt,
                                 crypto_aead_xchacha20poly1305_ietf_ABYTES,
                                 cipher, nonce.toMemView(), key.toMemView(),
                                 ad);
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::is_AES256GCM_avail()
    {
      return (sodium.crypto_aead_aes256gcm_is_available() == 1);
    }

    //----------------------------------------------------------------------------

    MemArray SodiumLib::aead_aes256gcm_encrypt(
        const MemView& msg, const SodiumLib::AEAD_AES256GCM_NonceType& nonce,
        const SodiumLib::AEAD_AES256GCM_KeyType& key, const MemView& ad)
    {
      if (sodium.crypto_aead_aes256gcm_is_available() != 1)
      {
        throw SodiumAES256GCMUnavail("crypto_aead_aes256gcm_encrypt");
      }

      return aead_encrypt(sodium.crypto_aead_aes256gcm_encrypt,
                                 crypto_aead_aes256gcm_ABYTES, msg, nonce.toMemView(),
                                 key.toMemView(), ad);
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::aead_aes256gcm_decrypt(
        const MemView& cipher, const SodiumLib::AEAD_AES256GCM_NonceType& nonce,
        const SodiumLib::AEAD_AES256GCM_KeyType& key, const MemView& ad, SodiumSecureMemType clearTextProtection)
    {
      if (sodium.crypto_aead_aes256gcm_is_available() != 1)
      {
        throw SodiumAES256GCMUnavail("crypto_aead_aes256gcm_decrypt");
      }

      return aead_decrypt(sodium.crypto_aead_aes256gcm_decrypt, crypto_aead_aes256gcm_ABYTES,
                                 cipher, nonce.toMemView(), key.toMemView(),
                                 ad, clearTextProtection);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::aead_aes256gcm_encrypt(const string& msg, const AEAD_AES256GCM_NonceType& nonce, const AEAD_AES256GCM_KeyType& key, const string& ad)
    {
      if (sodium.crypto_aead_aes256gcm_is_available() != 1)
      {
        throw SodiumAES256GCMUnavail("crypto_aead_aes256gcm_decrypt");
      }

      return aead_encrypt(sodium.crypto_aead_aes256gcm_encrypt,
                                 crypto_aead_aes256gcm_ABYTES,
                                 msg, nonce.toMemView(), key.toMemView(),
                                 ad);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::aead_aes256gcm_decrypt(const string& cipher, const SodiumLib::AEAD_AES256GCM_NonceType& nonce, const SodiumLib::AEAD_AES256GCM_KeyType& key, const string& ad)
    {
      if (sodium.crypto_aead_aes256gcm_is_available() != 1)
      {
        throw SodiumAES256GCMUnavail("crypto_aead_aes256gcm_decrypt");
      }

      return aead_decrypt(sodium.crypto_aead_aes256gcm_decrypt,
                                 crypto_aead_aes256gcm_ABYTES,
                                 cipher, nonce.toMemView(), key.toMemView(),
                                 ad);
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::genAsymCryptoKeyPair(AsymCrypto_PublicKey& pk_out, AsymCrypto_SecretKey& sk_out)
    {
      if (pk_out.empty()) return false;
      if (sk_out.empty()) return false;

      sodium.crypto_box_keypair(pk_out.to_ucPtr_rw(), sk_out.to_ucPtr_rw());
      return true;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::genAsymCryptoKeyPairSeeded(const SodiumLib::AsymCrypto_KeySeed& seed, AsymCrypto_PublicKey& pk_out, AsymCrypto_SecretKey& sk_out)
    {
      if (seed.empty())
      {
        return false;
      }

      sodium.crypto_box_seed_keypair(pk_out.to_ucPtr_rw(), sk_out.to_ucPtr_rw(), seed.to_ucPtr_ro());

      return true;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::genPublicCryptoKeyFromSecretKey(const SodiumLib::AsymCrypto_SecretKey& sk, AsymCrypto_PublicKey& pk_out)
    {
      if (pk_out.empty()) return false;
      if (sk.empty()) return false;

      sodium.crypto_scalarmult_base(pk_out.to_ucPtr_rw(), sk.to_ucPtr_ro());

      return true;
    }

    //----------------------------------------------------------------------------

    MemArray SodiumLib::box_easy(const MemView& msg, const SodiumLib::AsymCrypto_Nonce& nonce,
                                             const SodiumLib::AsymCrypto_PublicKey& recipientKey, const SodiumLib::AsymCrypto_SecretKey& senderKey)
    {
      // the message and the other parameters should be valid
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_box_easy");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_box_easy");
      }
      if (recipientKey.empty() || senderKey.empty())
      {
        throw SodiumInvalidKey("crypto_box_easy");
      }

      // allocate space for the result
      MemArray cipher{msg.size() + crypto_box_MACBYTES};

      // the actual encryption
      sodium.crypto_box_easy(cipher.to_ucPtr(), msg.to_ucPtr(), msg.size(), nonce.to_ucPtr_ro(),
                             recipientKey.to_ucPtr_ro(), senderKey.to_ucPtr_ro());

      return cipher;
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::box_open_easy(const MemView& cipher, const SodiumLib::AsymCrypto_Nonce& nonce,
                                                       const SodiumLib::AsymCrypto_PublicKey& senderKey, const SodiumLib::AsymCrypto_SecretKey& recipientKey,
                                                       SodiumSecureMemType clearTextProtection)
    {
      // the cipher and keys should be valid
      if (cipher.empty())
      {
        throw SodiumInvalidCipher("crypto_box_open_easy");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_box_open_easy");
      }
      if (recipientKey.empty() || senderKey.empty())
      {
        throw SodiumInvalidKey("crypto_box_open_easy");
      }

      // we need at least one message byte, so just the MAC is
      // not sufficient
      if (cipher.size() <= crypto_box_MACBYTES)
      {
        throw SodiumInvalidCipher("crypto_box_open_easy");
      }

      // allocate space for the result
      SodiumSecureMemory msg{cipher.size() - crypto_box_MACBYTES, clearTextProtection};

      // decrypt
      int isOkay = sodium.crypto_box_open_easy(msg.to_ucPtr_rw(), cipher.to_ucPtr(), cipher.size(), nonce.to_ucPtr_ro(),
                                               senderKey.to_ucPtr_ro(), recipientKey.to_ucPtr_ro());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? std::move(msg) : SodiumSecureMemory{};
    }

    //----------------------------------------------------------------------------

    pair<MemArray, SodiumLib::AsymCrypto_Tag> SodiumLib::box_detached(const MemView& msg, const SodiumLib::AsymCrypto_Nonce& nonce,
                                                                      const SodiumLib::AsymCrypto_PublicKey& recipientKey, const SodiumLib::AsymCrypto_SecretKey& senderKey)
    {
      // the message and the other parameters should be valid
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_box_detached");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_box_detached");
      }
      if (recipientKey.empty() || senderKey.empty())
      {
        throw SodiumInvalidKey("crypto_box_detached");
      }

      // allocate space for the cipher and the MAC
      MemArray cipher{msg.size()};
      AsymCrypto_Tag mac;

      // encrypt
      sodium.crypto_box_detached(cipher.to_ucPtr(), mac.to_ucPtr_rw(), msg.to_ucPtr(), msg.size(), nonce.to_ucPtr_ro(),
                                 recipientKey.to_ucPtr_ro(), senderKey.to_ucPtr_ro());

      return make_pair(std::move(cipher), std::move(mac));
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::box_open_detached(const MemView& cipher, const SodiumLib::AsymCrypto_Tag& mac, const SodiumLib::AsymCrypto_Nonce& nonce,
                                                           const SodiumLib::AsymCrypto_PublicKey& senderKey, const AsymCrypto_SecretKey& recipientKey,
                                                           SodiumSecureMemType clearTextProtection)
    {
      // the cipher, MAC and keys should be valid
      // the cipher and keys should be valid
      if (cipher.empty())
      {
        throw SodiumInvalidCipher("crypto_box_open_detached");
      }
      if (mac.empty())
      {
        throw SodiumInvalidCipher("crypto_box_open_detached");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_box_open_detached");
      }
      if (recipientKey.empty() || senderKey.empty())
      {
        throw SodiumInvalidKey("crypto_box_open_detached");
      }

      // allocate space for the result
      SodiumSecureMemory msg{cipher.size(), clearTextProtection};

      // decrypt
      int isOkay = sodium.crypto_box_open_detached(msg.to_ucPtr_rw(), cipher.to_ucPtr(), mac.to_ucPtr_ro(), cipher.size(),
                                                   nonce.to_ucPtr_ro(), senderKey.to_ucPtr_ro(), recipientKey.to_ucPtr_ro());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? std::move(msg) : SodiumSecureMemory{};
    }

    //----------------------------------------------------------------------------

    string SodiumLib::box_easy(const string& msg, const SodiumLib::AsymCrypto_Nonce& nonce, const SodiumLib::AsymCrypto_PublicKey& recipientKey,
                                      const SodiumLib::AsymCrypto_SecretKey& senderKey)
    {
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_box_easy");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_box_easy");
      }
      if (recipientKey.empty() || senderKey.empty())
      {
        throw SodiumInvalidKey("crypto_box_easy");
      }

      // allocate space for the result
      string cipher;
      cipher.resize(msg.size() + crypto_box_MACBYTES);

      // the actual encryption
      sodium.crypto_box_easy((unsigned char*)cipher.c_str(), reinterpret_cast<const unsigned char*>(msg.c_str()), msg.size(),
                             nonce.to_ucPtr_ro(), recipientKey.to_ucPtr_ro(), senderKey.to_ucPtr_ro());

      return cipher;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::box_open_easy(const string& cipher, const SodiumLib::AsymCrypto_Nonce& nonce, const SodiumLib::AsymCrypto_PublicKey& senderKey,
                                           const AsymCrypto_SecretKey& recipientKey)
    {
      // the cipher and keys should be valid
      if (cipher.size() <= crypto_box_MACBYTES)
      {
        throw SodiumInvalidCipher("crypto_box_open_easy");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_box_open_easy");
      }
      if (recipientKey.empty() || senderKey.empty())
      {
        throw SodiumInvalidKey("crypto_box_open_easy");
      }

      // allocate space for the result
      string msg;
      msg.resize(cipher.size() - crypto_box_MACBYTES);

      // decrypt
      int isOkay = sodium.crypto_box_open_easy((unsigned char*)msg.c_str(), reinterpret_cast<const unsigned char*>(cipher.c_str()), cipher.size(),
                                               nonce.to_ucPtr_ro(), senderKey.to_ucPtr_ro(), recipientKey.to_ucPtr_ro());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? msg : string{};
    }

    //----------------------------------------------------------------------------

    pair<string, SodiumLib::AsymCrypto_Tag> SodiumLib::box_detached(const string& msg, const SodiumLib::AsymCrypto_Nonce& nonce,
                                                        const SodiumLib::AsymCrypto_PublicKey& recipientKey, const SodiumLib::AsymCrypto_SecretKey& senderKey)
    {
      // the message and the other parameters should be valid
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_box_detached");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_box_detached");
      }
      if (recipientKey.empty() || senderKey.empty())
      {
        throw SodiumInvalidKey("crypto_box_detached");
      }

      // allocate space for the cipher and the MAC
      string cipher;
      cipher.resize(msg.size());
      AsymCrypto_Tag mac;

      // encrypt
      sodium.crypto_box_detached((unsigned char*)cipher.c_str(), mac.to_ucPtr_rw(),
                                 reinterpret_cast<const unsigned char*>(msg.c_str()), msg.size(),
                                 nonce.to_ucPtr_ro(), recipientKey.to_ucPtr_ro(), senderKey.to_ucPtr_ro());

      return make_pair(std::move(cipher), std::move(mac));
    }

    //----------------------------------------------------------------------------

    string SodiumLib::box_open_detached(const string& cipher, const AsymCrypto_Tag& mac, const AsymCrypto_Nonce& nonce,
                                               const AsymCrypto_PublicKey& senderKey, const AsymCrypto_SecretKey& recipientKey)
    {
      // the cipher, MAC and keys should be valid
      // the cipher and keys should be valid
      if (cipher.empty())
      {
        throw SodiumInvalidCipher("crypto_box_open_detached");
      }
      if (mac.empty())
      {
        throw SodiumInvalidCipher("crypto_box_open_detached");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidNonce("crypto_box_open_detached");
      }
      if (recipientKey.empty() || senderKey.empty())
      {
        throw SodiumInvalidKey("crypto_box_open_detached");
      }

      // allocate space for the result
      string msg;
      msg.resize(cipher.size());

      // decrypt
      int isOkay = sodium.crypto_box_open_detached((unsigned char*)msg.c_str(), reinterpret_cast<const unsigned char*>(cipher.c_str()),
                                                   mac.to_ucPtr_ro(), cipher.size(),
                                                   nonce.to_ucPtr_ro(), senderKey.to_ucPtr_ro(), recipientKey.to_ucPtr_ro());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? msg : string{};
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::genAsymSignKeyPair(SodiumLib::AsymSign_PublicKey& pk_out, SodiumLib::AsymSign_SecretKey& sk_out)
    {
      if (pk_out.empty()) return false;
      if (sk_out.empty()) return false;

      sodium.crypto_sign_keypair(pk_out.to_ucPtr_rw(), sk_out.to_ucPtr_rw());

      return true;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::genAsymSignKeyPairSeeded(const SodiumLib::AsymSign_KeySeed& seed, SodiumLib::AsymSign_PublicKey& pk_out, SodiumLib::AsymSign_SecretKey& sk_out)
    {
      if (seed.empty()) return false;
      if (pk_out.empty()) return false;
      if (sk_out.empty()) return false;

      sodium.crypto_sign_seed_keypair(pk_out.to_ucPtr_rw(), sk_out.to_ucPtr_rw(), seed.to_ucPtr_ro());
      return true;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::genPublicSignKeyFromSecretKey(const SodiumLib::AsymSign_SecretKey& sk, SodiumLib::AsymSign_PublicKey& pk_out)
    {
      if (pk_out.empty()) return false;
      if (sk.empty()) return false;

      sodium.crypto_sign_ed25519_sk_to_pk(pk_out.to_ucPtr_rw(), sk.to_ucPtr_ro());
      return true;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::genSignKeySeedFromSecretKey(const SodiumLib::AsymSign_SecretKey& sk, SodiumLib::AsymSign_KeySeed& seed_out)
    {
      if (sk.empty()) return false;
      if (seed_out.empty()) return false;

      sodium.crypto_sign_ed25519_sk_to_seed(seed_out.to_ucPtr_rw(), sk.to_ucPtr_ro());
      return true;
    }

    //----------------------------------------------------------------------------

    MemArray SodiumLib::sign(const MemView& msg, const SodiumLib::AsymSign_SecretKey& sk)
    {
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_sign");
      }
      if (sk.empty())
      {
        throw SodiumInvalidKey("crypto_sign");
      }

      // allocate space for the result
      MemArray signedMsg{crypto_sign_BYTES + msg.size()};

      // sign
      sodium.crypto_sign(signedMsg.to_ucPtr(), nullptr, msg.to_ucPtr(), msg.size(), sk.to_ucPtr_ro());

      return signedMsg;
    }

    //----------------------------------------------------------------------------

    MemArray SodiumLib::sign_open(const MemView& signedMsg, const SodiumLib::AsymSign_PublicKey& pk)
    {
      if ((signedMsg.empty()) || (signedMsg.size() <= crypto_sign_BYTES))
      {
        throw SodiumInvalidMessage("crypto_sign_open");
      }
      if (pk.empty())
      {
        throw SodiumInvalidKey("crypto_sign_open");
      }

      // allocate space for the result
      MemArray msg{signedMsg.size() - crypto_sign_BYTES};

      // check signature and remove it from the signed message
      int rc = sodium.crypto_sign_open(msg.to_ucPtr(), nullptr, signedMsg.to_ucPtr(), signedMsg.size(), pk.to_ucPtr_ro());

      return (rc == 0) ? std::move(msg) : MemArray{};
    }

    //----------------------------------------------------------------------------

    SodiumLib::AsymSign_Signature SodiumLib::sign_detached(const MemView& msg, const SodiumLib::AsymSign_SecretKey& sk)
    {
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_sign");
      }
      if (sk.empty())
      {
        throw SodiumInvalidKey("crypto_sign");
      }

      // sign
      AsymSign_Signature sig;
      sodium.crypto_sign_detached(sig.to_ucPtr_rw(), nullptr, msg.to_ucPtr(), msg.size(), sk.to_ucPtr_ro());

      return sig;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::sign_verify_detached(const MemView& msg, const AsymSign_Signature& sig, const SodiumLib::AsymSign_PublicKey& pk)
    {
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_sign_verify_detached");
      }
      if (sig.empty())
      {
        throw SodiumInvalidMac("crypto_sign_verify_detached");
      }
      if (pk.empty())
      {
        throw SodiumInvalidKey("crypto_sign_verify_detached");
      }

      // check signature
      int rc = sodium.crypto_sign_verify_detached(sig.to_ucPtr_ro(), msg.to_ucPtr(), msg.size(), pk.to_ucPtr_ro());

      return (rc == 0);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::sign(const string& msg, const SodiumLib::AsymSign_SecretKey& sk)
    {
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_sign");
      }
      if (sk.empty())
      {
        throw SodiumInvalidKey("crypto_sign");
      }

      // allocate space for the result
      string signedMsg;
      signedMsg.resize(crypto_sign_BYTES + msg.size());

      // sign
      sodium.crypto_sign((unsigned char*)signedMsg.c_str(), nullptr, reinterpret_cast<const unsigned char*>(msg.c_str()), msg.size(), sk.to_ucPtr_ro());

      return signedMsg;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::sign_open(const string& signedMsg, const SodiumLib::AsymSign_PublicKey& pk)
    {
      // check input validity
      if (signedMsg.size() <= crypto_sign_BYTES)
      {
        throw SodiumInvalidMessage("crypto_sign_open");
      }
      if (pk.empty())
      {
        throw SodiumInvalidKey("crypto_sign_open");
      }

      // allocate space for the result
      string msg;
      msg.resize(signedMsg.size() - crypto_sign_BYTES);

      // check signature and remove it from the signed message
      int rc = sodium.crypto_sign_open((unsigned char*)msg.c_str(), nullptr, reinterpret_cast<const unsigned char*>(signedMsg.c_str()),
                                       signedMsg.size(), pk.to_ucPtr_ro());

      return (rc == 0) ? msg : string{};
    }

    //----------------------------------------------------------------------------

    SodiumLib::AsymSign_Signature SodiumLib::sign_detached(const string& msg, const SodiumLib::AsymSign_SecretKey& sk)
    {
      MemView mv{msg};
      return sign_detached(mv, sk);
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::sign_verify_detached(const string& msg, const AsymSign_Signature& sig, const SodiumLib::AsymSign_PublicKey& pk)
    {
      MemView mv{msg};
      return sign_verify_detached(mv, sig, pk);
    }

    //----------------------------------------------------------------------------

    MemArray SodiumLib::generichash(const MemView& inData, size_t hashLen)
    {
      if (inData.empty())
      {
        throw SodiumInvalidMessage("crypto_generichash");
      }
      if ((hashLen < crypto_generichash_BYTES_MIN) || (hashLen > crypto_generichash_BYTES_MAX))
      {
        throw std::range_error("crypto_generichash: invalid hash length requested");
      }

      // allocate space for the result
      MemArray hash{hashLen};

      // calculate the hash
      sodium.crypto_generichash(hash.to_ucPtr(), hashLen, inData.to_ucPtr(), inData.size(), nullptr, 0);

      return hash;
    }

    //----------------------------------------------------------------------------

    MemArray SodiumLib::generichash(const MemView& inData, const SodiumLib::GenericHashKey& key, size_t hashLen)
    {
      if (inData.empty())
      {
        throw SodiumInvalidMessage("crypto_generichash");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_generichash");
      }
      if ((hashLen < crypto_generichash_BYTES_MIN) || (hashLen > crypto_generichash_BYTES_MAX))
      {
        throw std::range_error("crypto_generichash: invalid hash length requested");
      }

      // allocate space for the result
      MemArray hash{hashLen};

      // calculate the hash
      sodium.crypto_generichash(hash.to_ucPtr(), hashLen, inData.to_ucPtr(), inData.size(),
                                key.to_ucPtr_ro(), key.size());

      return hash;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::generichash(const string& inData, size_t hashLen)
    {
      if (inData.empty())
      {
        throw SodiumInvalidMessage("crypto_generichash");
      }
      if ((hashLen < crypto_generichash_BYTES_MIN) || (hashLen > crypto_generichash_BYTES_MAX))
      {
        throw std::range_error("crypto_generichash: invalid hash length requested");
      }

      string hash;
      hash.resize(hashLen);

      sodium.crypto_generichash((unsigned char*)hash.c_str(), hashLen,
                                reinterpret_cast<const unsigned char*>(inData.c_str()), inData.size(), nullptr, 0);

      return hash;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::generichash(const string& inData, const SodiumLib::GenericHashKey& key, size_t hashLen)
    {
      if (inData.empty())
      {
        throw SodiumInvalidMessage("crypto_generichash");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_generichash");
      }
      if ((hashLen < crypto_generichash_BYTES_MIN) || (hashLen > crypto_generichash_BYTES_MAX))
      {
        throw std::range_error("crypto_generichash: invalid hash length requested");
      }

      string hash;
      hash.resize(crypto_generichash_BYTES);

      sodium.crypto_generichash((unsigned char*)hash.c_str(), crypto_generichash_BYTES,
                                reinterpret_cast<const unsigned char*>(inData.c_str()), inData.size(),
                                key.to_ucPtr_ro(), key.size());

      return hash;
    }

    //----------------------------------------------------------------------------

    void SodiumLib::generichash_init(crypto_generichash_state* state, size_t hashLen)
    {
      if (state == nullptr)
      {
        throw std::invalid_argument("crypto_generichash_init() received nullptr for state variable");
      }

      sodium.crypto_generichash_init(state, nullptr, 0, hashLen);
    }

    //----------------------------------------------------------------------------

    void SodiumLib::generichash_init(crypto_generichash_state* state, const GenericHashKey& key, size_t hashLen)
    {
      if (state == nullptr)
      {
        throw std::invalid_argument("crypto_generichash_init() received nullptr for state variable");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_generichash");
      }
      if ((hashLen < crypto_generichash_BYTES_MIN) || (hashLen > crypto_generichash_BYTES_MAX))
      {
        throw std::range_error("crypto_generichash: invalid hash length requested");
      }

      sodium.crypto_generichash_init(state, key.to_ucPtr_ro(), key.size(), hashLen);
    }

    //----------------------------------------------------------------------------

    void SodiumLib::generichash_update(crypto_generichash_state* state, const MemView& inData)
    {
      if (state == nullptr)
      {
        throw std::invalid_argument("crypto_generichash_update() received nullptr for state variable");
      }
      if (inData.empty())
      {
        throw SodiumInvalidMessage("crypto_generichash_update");
      }

      sodium.crypto_generichash_update(state, inData.to_ucPtr(), inData.size());
    }

    //----------------------------------------------------------------------------

    void SodiumLib::generichash_update(crypto_generichash_state* state, const string& inData)
    {
      generichash_update(state, MemView{inData});
    }

    //----------------------------------------------------------------------------

    MemArray SodiumLib::generichash_final(crypto_generichash_state* state, size_t hashLen)
    {
      if (state == nullptr)
      {
        throw std::invalid_argument("crypto_generichash_final() received nullptr for state variable");
      }
      if ((hashLen < crypto_generichash_BYTES_MIN) || (hashLen > crypto_generichash_BYTES_MAX))
      {
        throw std::range_error("crypto_generichash: invalid hash length requested");
      }

      // allocate space for the result
      MemArray hash{crypto_generichash_BYTES};

      // finalize and get the hash
      sodium.crypto_generichash_final(state, hash.to_ucPtr(), hashLen);

      return hash;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::generichash_final_string(crypto_generichash_state* state, size_t hashLen)
    {
      if (state == nullptr)
      {
        throw std::invalid_argument("crypto_generichash_final() received nullptr for state variable");
      }
      if ((hashLen < crypto_generichash_BYTES_MIN) || (hashLen > crypto_generichash_BYTES_MAX))
      {
        throw std::range_error("crypto_generichash: invalid hash length requested");
      }

      // allocate space for the result
      string hash;
      hash.resize(hashLen);

      // finalize and get the hash
      sodium.crypto_generichash_final(state, (unsigned char*)hash.c_str(), hashLen);

      return hash;
    }

    //----------------------------------------------------------------------------

    MemArray SodiumLib::shorthash(const MemView& inData, const SodiumLib::ShorthashKey& key)
    {
      if (inData.empty())
      {
        throw SodiumInvalidMessage("crypto_shorthash");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_shorthash");
      }

      // allocate space for the result
      MemArray hash{crypto_shorthash_BYTES};

      // get the hash
      sodium.crypto_shorthash(hash.to_ucPtr(), inData.to_ucPtr(), inData.size(), key.to_ucPtr_ro());

      return hash;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::shorthash(const string& inData, const SodiumLib::ShorthashKey& key)
    {
      if (inData.empty())
      {
        throw SodiumInvalidMessage("crypto_shorthash");
      }
      if (key.empty())
      {
        throw SodiumInvalidKey("crypto_shorthash");
      }

      // allocate space for the result
      string hash;
      hash.resize(crypto_shorthash_BYTES);

      // get the hash
      sodium.crypto_shorthash((unsigned char*)hash.c_str(),
                              reinterpret_cast<const unsigned char*>(inData.c_str()), inData.size(),
                              key.to_ucPtr_ro());

      return hash;
    }

    //----------------------------------------------------------------------------

    pair<SodiumSecureMemory, SodiumLib::PwHashData> SodiumLib::pwhash(const MemView& pw, size_t hashLen, SodiumLib::PasswdHashStrength strength,
                                                                     SodiumLib::PasswdHashAlgo algo, SodiumSecureMemType memType)
    {
      // construct a new PwHashData struct with the hashing data.
      // the salt is empty and will thus be filled with random data
      PwHashData hDat{};
      hDat.algo = algo;

      // determine opslimit and memlimit
      tie(hDat.opslimit, hDat.memlimit) = pwHashConfigToValues(strength);

      // fill the salt with random data
      randombytes_buf(hDat.salt.toNotOwningArray());

      // call the hash function
      SodiumSecureMemory hash = pwhash(pw, hashLen, hDat, memType);

      return make_pair(std::move(hash), std::move(hDat));
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::pwhash(const MemView& pw, size_t hashLen, PwHashData& hDat, SodiumSecureMemType memType)
    {
      if (pw.empty())
      {
        throw SodiumInvalidBuffer("crypto_pwhash");
      }
      if ((hashLen < crypto_pwhash_BYTES_MIN) || (hashLen > crypto_pwhash_BYTES_MAX))
      {
        throw std::range_error("crypto_pwhash: invalid hash length requested");
      }

      // allocate the result buffer
      SodiumSecureMemory hash{hashLen, memType};

      // call the hash function
      int rc = -1;
      rc = sodium.crypto_pwhash(hash.to_ucPtr_rw(), hashLen, pw.to_charPtr(), pw.size(),
                                hDat.salt.to_ucPtr_ro(), hDat.opslimit, hDat.memlimit, PasswdHashAlgo2Int(hDat.algo));
      return (rc == 0) ? std::move(hash) : SodiumSecureMemory{};
    }

    //----------------------------------------------------------------------------

    string SodiumLib::pwhash_str(const MemView& pw, SodiumLib::PasswdHashStrength strength)
    {
      if (pw.empty())
      {
        throw SodiumInvalidBuffer("crypto_pwhash_str");
      }

      unsigned long long opslimit;
      size_t memlimit;
      tie(opslimit, memlimit) = pwHashConfigToValues(strength);

      int rc = -1;
      char s[crypto_pwhash_STRBYTES];
      rc = sodium.crypto_pwhash_str(&(s[0]), pw.to_charPtr(), pw.size(), opslimit, memlimit);

      return (rc == 0) ? string{s} : string{};
    }

    //----------------------------------------------------------------------------

    string SodiumLib::pwhash_str(const string& pw, SodiumLib::PasswdHashStrength strength)
    {
      return pwhash_str(MemView{pw}, strength);
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::pwhash_str_verify(const MemView& pw, const string& hashResult)
    {
      int rc = -1;
      rc = sodium.crypto_pwhash_str_verify(hashResult.c_str(), pw.to_charPtr(), pw.size());

      return (rc == 0);
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::pwhash_str_verify(const string& pw, const string& hashResult)
    {
      return pwhash_str_verify(MemView{pw}, hashResult);
    }

    //----------------------------------------------------------------------------

    pair<SodiumLib::DH_SecretKey, SodiumLib::DH_PublicKey> SodiumLib::genDHKeyPair()
    {
      DH_SecretKey sk;
      randombytes_buf(sk.toNotOwningArray());

      DH_PublicKey pk = genPublicDHKeyFromSecretKey(sk);
      return make_pair(std::move(sk), std::move(pk));
    }

    //----------------------------------------------------------------------------

    SodiumLib::DH_SharedSecret SodiumLib::genDHSharedSecret(const DH_SecretKey& mySecretKey, const DH_PublicKey& othersPublicKey)
    {
      if (mySecretKey.empty() || othersPublicKey.empty())
      {
        throw SodiumInvalidKey("genDHSharedSecret");
      }

      DH_SharedSecret sh;

      sodium.crypto_scalarmult(sh.to_ucPtr_rw(), mySecretKey.to_ucPtr_ro(), othersPublicKey.to_ucPtr_ro());
      return sh;
    }

    //----------------------------------------------------------------------------

    SodiumLib::DH_PublicKey SodiumLib::genPublicDHKeyFromSecretKey(const SodiumLib::DH_SecretKey& sk)
    {
      if (sk.empty())
      {
        throw SodiumInvalidKey("genPublicDHKeyFromSecretKey");
      }

      DH_PublicKey pk;

      sodium.crypto_scalarmult_base(pk.to_ucPtr_rw(), sk.to_ucPtr_ro());

      return pk;
    }

    //----------------------------------------------------------------------------

    pair<SodiumLib::KX_SecretKey, SodiumLib::KX_PublicKey> SodiumLib::genKeyExchangeKeyPair()
    {
      KX_SecretKey sk;
      KX_PublicKey pk;

      sodium.crypto_kx_keypair(pk.to_ucPtr_rw(), sk.to_ucPtr_rw());

      return make_pair(std::move(sk), std::move(pk));
    }

    //----------------------------------------------------------------------------

    pair<SodiumLib::KX_SecretKey, SodiumLib::KX_PublicKey> SodiumLib::genKeyExchangeKeyPair(const SodiumLib::KX_KeySeed& seed)
    {
      if (seed.empty())
      {
        throw SodiumInvalidKey("genKeyExchangeKeyPair with seed");
      }

      KX_SecretKey sk;
      KX_PublicKey pk;

      sodium.crypto_kx_seed_keypair(pk.to_ucPtr_rw(), sk.to_ucPtr_rw(), seed.to_ucPtr_ro());

      return make_pair(std::move(sk), std::move(pk));
    }

    //----------------------------------------------------------------------------

    pair<SodiumLib::KX_SessionKey, SodiumLib::KX_SessionKey> SodiumLib::getClientSessionKeys(const SodiumLib::KX_PublicKey& clientPubKey, const SodiumLib::KX_SecretKey& clientSecKey, const SodiumLib::KX_PublicKey& serverPubKey)
    {
      if (clientPubKey.empty() || clientSecKey.empty() || serverPubKey.empty())
      {
        throw SodiumInvalidKey("getClientSessionKeys");
      }

      KX_SessionKey rx;
      KX_SessionKey tx;
      int rc = sodium.crypto_kx_client_session_keys(
            rx.to_ucPtr_rw(), tx.to_ucPtr_rw(),
            clientPubKey.to_ucPtr_ro(), clientSecKey.to_ucPtr_ro(),
            serverPubKey.to_ucPtr_ro());

      if (rc != 0)
      {
        throw SodiumInvalidKey("getClientSessionKeys");
      }

      return make_pair(std::move(rx), std::move(tx));
    }

    //----------------------------------------------------------------------------

    pair<SodiumLib::KX_SessionKey, SodiumLib::KX_SessionKey> SodiumLib::getServerSessionKeys(const SodiumLib::KX_PublicKey& serverPubKey, const SodiumLib::KX_SecretKey& serverSecKey, const SodiumLib::KX_PublicKey& clientPubKey)
    {
      if (serverPubKey.empty() || serverSecKey.empty() || clientPubKey.empty())
      {
        throw SodiumInvalidKey("getServerSessionKeys");
      }

      KX_SessionKey rx;
      KX_SessionKey tx;
      int rc = sodium.crypto_kx_server_session_keys(
            rx.to_ucPtr_rw(), tx.to_ucPtr_rw(),
            serverPubKey.to_ucPtr_ro(), serverSecKey.to_ucPtr_ro(),
            clientPubKey.to_ucPtr_ro());

      if (rc != 0)
      {
        throw SodiumInvalidKey("getClientSessionKeys");
      }

      return make_pair(std::move(rx), std::move(tx));
    }


    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    SodiumSecretBox::SodiumSecretBox(const SodiumLib::SecretBoxKey& _key, const SodiumLib::SecretBoxNonce& _nonce, bool autoIncNonce)
      :lib{SodiumLib::getInstance()}, key{_key}, nonce{_nonce}, autoIncrementNonce{autoIncNonce}
    {
      if (key.empty())
      {
        throw SodiumInvalidKey("ctor SodiumSecretBox");
      }

      if (!(key.setAccess(SodiumSecureMemAccess::NoAccess)))
      {
        throw SodiumMemoryGuardException{"ctor SodiumSecretBox, could not guard private key"};
      }
    }

    //----------------------------------------------------------------------------

    MemArray SodiumSecretBox::encryptCombined(const MemView& msg)
    {
      setKeyLockState(false);
      MemArray result = lib->secretbox_easy(msg, nonce.getNonceAsRef(), key);
      setKeyLockState(true);

      if (autoIncrementNonce) nonce.increment();

      return result;
    }

    //----------------------------------------------------------------------------

    pair<MemArray, SodiumLib::SecretBoxMac> SodiumSecretBox::encryptDetached(const MemView& msg)
    {
      setKeyLockState(false);
      auto result = lib->secretbox_detached(msg, nonce.getNonceAsRef(), key);
      setKeyLockState(true);

      if (autoIncrementNonce) nonce.increment();

      return result;
    }

    //----------------------------------------------------------------------------

    string SodiumSecretBox::encryptCombined(const string& msg)
    {
      setKeyLockState(false);
      string result = lib->secretbox_easy(msg, nonce.getNonceAsRef(), key);
      setKeyLockState(true);

      if (autoIncrementNonce) nonce.increment();

      return result;
    }

    //----------------------------------------------------------------------------

    pair<string, SodiumLib::SecretBoxMac> SodiumSecretBox::encryptDetached(const string& msg)
    {
      setKeyLockState(false);
      auto result = lib->secretbox_detached(msg, nonce.getNonceAsRef(), key);
      setKeyLockState(true);

      if (autoIncrementNonce) nonce.increment();

      return make_pair(std::move(result.first), std::move(result.second));
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumSecretBox::decryptCombined(const MemView& cipher, SodiumSecureMemType clearTextProtection)
    {
      setKeyLockState(false);
      SodiumSecureMemory result = lib->secretbox_open_easy__secure(cipher, nonce.getNonceAsRef(), key, clearTextProtection);
      setKeyLockState(true);

      if (autoIncrementNonce) nonce.increment();

      return result;
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumSecretBox::decryptDetached(const MemView& cipher, const SodiumLib::SecretBoxMac& mac, SodiumSecureMemType clearTextProtection)
    {
      setKeyLockState(false);
      SodiumSecureMemory result = lib->secretbox_open_detached(cipher, mac, nonce.getNonceAsRef(), key, clearTextProtection);
      setKeyLockState(true);

      if (autoIncrementNonce) nonce.increment();

      return result;
    }

    //----------------------------------------------------------------------------

    string SodiumSecretBox::decryptCombined(const string& cipher)
    {
      setKeyLockState(false);
      string msg = lib->secretbox_open_easy(cipher, nonce.getNonceAsRef(), key);
      setKeyLockState(true);

      if (autoIncrementNonce) nonce.increment();

      return msg;
    }

    //----------------------------------------------------------------------------

    string SodiumSecretBox::decryptDetached(const string& cipher, const SodiumLib::SecretBoxMac& mac)
    {
      setKeyLockState(false);
      string result = lib->secretbox_open_detached(cipher, mac, nonce.getNonceAsRef(), key);
      setKeyLockState(true);

      if (autoIncrementNonce) nonce.increment();

      return result;
    }

    //----------------------------------------------------------------------------

    void SodiumSecretBox::setKeyLockState(bool setGuard)
    {
      SodiumSecureMemAccess newState = setGuard ? SodiumSecureMemAccess::NoAccess : SodiumSecureMemAccess::RO;

      if (!(key.setAccess(newState)))
      {
        throw SodiumMemoryGuardException{"SecretBox, could not guard / unlock secret key"};
      }
    }

    //----------------------------------------------------------------------------

    GenericHasher::GenericHasher()
      :lib{SodiumLib::getInstance()}
    {
      if (lib == nullptr)
      {
        throw SodiumNotAvailableException{};
      }

      lib->generichash_init(&state, outLen);
    }

    //----------------------------------------------------------------------------

    GenericHasher::GenericHasher(size_t hashLen)
      :GenericHasher{}
    {
      outLen = hashLen;
      lib->generichash_init(&state, outLen);
    }

    //----------------------------------------------------------------------------

    GenericHasher::GenericHasher(const SodiumLib::GenericHashKey& k, size_t hashLen)
      :lib{SodiumLib::getInstance()}, outLen{hashLen}, isFinalized{false}
    {
      if (lib == nullptr)
      {
        throw SodiumNotAvailableException{};
      }

      lib->generichash_init(&state, k, outLen);
    }

    //----------------------------------------------------------------------------

    void GenericHasher::append(const MemView& inData)
    {
      if (isFinalized) return;

      lib->generichash_update(&state, inData);
    }

    //----------------------------------------------------------------------------

    void GenericHasher::append(const string& inData)
    {
      if (isFinalized) return;

      lib->generichash_update(&state, inData);
    }

    //----------------------------------------------------------------------------

    MemArray GenericHasher::finalize()
    {
      if (isFinalized) return MemArray{};

      isFinalized = true;
      return lib->generichash_final(&state, outLen);
    }

    //----------------------------------------------------------------------------

    string GenericHasher::finalize_string()
    {
      if (isFinalized) return string{};

      isFinalized = true;
      return lib->generichash_final_string(&state, outLen);
    }

    //----------------------------------------------------------------------------

    DiffieHellmannExchanger::DiffieHellmannExchanger(bool _isClient)
      :isClient{_isClient}, lib{SodiumLib::getInstance()}
    {
      if (lib == nullptr)
      {
        throw SodiumNotAvailableException{};
      }

      tie(sk, pk) = lib->genDHKeyPair();
      sk.setAccess(SodiumSecureMemAccess::NoAccess);
    }

    //----------------------------------------------------------------------------

    SodiumLib::DH_PublicKey DiffieHellmannExchanger::getMyPublicKey()
    {
      return SodiumLib::DH_PublicKey{pk};
    }

    //----------------------------------------------------------------------------

    SodiumLib::DH_SharedSecret DiffieHellmannExchanger::getSharedSecret(const SodiumLib::DH_PublicKey& othersPublicKey)
    {
      ;

      sk.setAccess(SodiumSecureMemAccess::RO);
      SodiumLib::DH_SharedSecret shared = lib->genDHSharedSecret(sk, othersPublicKey);
      sk.setAccess(SodiumSecureMemAccess::NoAccess);

      if (shared.empty())
      {
        throw runtime_error("Damn, couldn't calculate Diffie-Hellmann shared secret. FIX: add a special exception for this!");
      }

      // hash this result with the two public keys
      GenericHasher hasher(shared.size());
      hasher.append(shared.toMemView());
      if (isClient)
      {
        hasher.append(pk.toMemView());
        hasher.append(othersPublicKey.toMemView());
      } else {
        hasher.append(othersPublicKey.toMemView());
        hasher.append(pk.toMemView());
      }

      SodiumLib::DH_SharedSecret result;
      result.fillFromString(hasher.finalize_string());

      return result;
    }

    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    PasswordProtectedSecret::PasswordProtectedSecret(SodiumLib::PasswdHashStrength pwStrength, SodiumLib::PasswdHashAlgo pwAlgo)
      :lib{SodiumLib::getInstance()}
    {
      // initialize the hash configuration
      tie(hashConfig.opslimit, hashConfig.memlimit) = lib->pwHashConfigToValues(pwStrength);
      hashConfig.algo = pwAlgo;
      lib->randombytes_buf(hashConfig.salt.toNotOwningArray());

      // set an initial nonce although this will never be used.
      // but if don't do this now, a subsequent call to asString()
      // would produce a malformed string
      lib->randombytes_buf(nonce.toNotOwningArray());
    }

    //----------------------------------------------------------------------------

    PasswordProtectedSecret::PasswordProtectedSecret(const string& data, bool isBase64)
      :lib{SodiumLib::getInstance()}
    {
      if (data.empty())
      {
        throw invalid_argument("Empty encrypted data for PasswordProtectedSecret ctor!");
      }

      string rawData = isBase64 ? fromBase64(data) : data;

      // read the hash config from the raw data. The following code throws
      // if the data was malformed
      try
      {
        Net::InMessage dissector{rawData};

        // determine the algo type
        uint8_t algoId = dissector.getByte();
        hashConfig.algo = static_cast<SodiumLib::PasswdHashAlgo>(algoId);

        // get memlimit and opslimit
        static_assert(sizeof(unsigned long long) == sizeof(size_t), "Invalid size of unsigned long long!");
        hashConfig.memlimit = dissector.getUI64();
        hashConfig.opslimit = dissector.getUI64();

        // read the salt
        if (!(hashConfig.salt.fillFromMemView(dissector.getMemView())))
        {
          throw std::invalid_argument("Invalid salt in ctor of PasswordProtectedSecret!");
        }

        // what remains are the cipher and nonce
        cipher = dissector.getMemArray();

        if (!(nonce.fillFromMemView(dissector.getMemView())))
        {
          throw std::invalid_argument("Invalid nonce in ctor of PasswordProtectedSecret!");
        }
      }
      catch (...)
      {
        throw MalformedEncryptedData{};
      }
    }

    //----------------------------------------------------------------------------

    bool PasswordProtectedSecret::setSecret(const MemView& sec)
    {
      if (sec.empty())
      {
        cipher.releaseMemory();
        return true;
      }

      if (pwClear.empty() || symKey.empty())
      {
        throw NoPasswordSet{};
      }

      // prepare a fresh random nonce for
      // each symmetric encryption step
      lib->randombytes_buf(nonce.toNotOwningArray());

      // do the encryption
      symKey.setAccess(SodiumSecureMemAccess::RO);
      MemArray _cipher = lib->secretbox_easy(sec, nonce, symKey);
      symKey.setAccess(SodiumSecureMemAccess::NoAccess);

      // only store the result if the encryption was successful
      if (_cipher.empty()) return false;
      cipher = std::move(_cipher);

      return true;
    }

    //----------------------------------------------------------------------------

    bool PasswordProtectedSecret::setSecret(const string& sec)
    {
      return setSecret(MemView{sec});
    }

    //----------------------------------------------------------------------------

    string PasswordProtectedSecret::getSecretAsString()
    {
      SodiumSecureMemory sec = getSecret(SodiumSecureMemType::Normal);

      if (sec.empty()) return string{};

      string result{sec.toMemView().to_charPtr(), sec.size()};

      return result;
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory PasswordProtectedSecret::getSecret(SodiumSecureMemType memType)
    {
      // is a decryption key available?
      if (pwClear.empty() || symKey.empty())
      {
        throw NoPasswordSet{};
      }

      // do we have any content at all?
      if (cipher.empty())
      {
        return SodiumSecureMemory{};
      }

      // decrypt and throw an exception if things fail
      symKey.setAccess(SodiumSecureMemAccess::RO);
      SodiumSecureMemory sec = lib->secretbox_open_easy__secure(cipher.view(), nonce, symKey, memType);
      symKey.setAccess(SodiumSecureMemAccess::NoAccess);

      if (sec.empty())
      {
        throw WrongPassword{};
      }

      return sec;
    }

    //----------------------------------------------------------------------------

    bool PasswordProtectedSecret::changePassword(const string& oldPw, const string& newPw, SodiumLib::PasswdHashStrength pwStrength, SodiumLib::PasswdHashAlgo pwAlgo)
    {      
      // the new password should not be empty
      if (newPw.empty()) return false;

      // check the validity of the old pw. this includes a check
      // whether a password is stored at all
      if (!(isValidPassword(oldPw))) return false;

      // decrypt the secret and store it in a temporary buffer
      SodiumSecureMemory sec = getSecret(SodiumSecureMemType::Normal);
      if (sec.empty() && (!(cipher.empty()))) return false;   // could not decrypt existing secret

      // determine the updated hashing parameters
      tie(hashConfig.opslimit, hashConfig.memlimit) = lib->pwHashConfigToValues(pwStrength);
      hashConfig.algo = pwAlgo;
      lib->randombytes_buf(hashConfig.salt.toNotOwningArray());

      // derive a new key from the password
      password2SymKey(newPw);
      cipher.releaseMemory();

      // re-store the secret
      if (!(sec.empty()))
      {
        return setSecret(sec.toMemView());
      }

      return true;
    }

    //----------------------------------------------------------------------------

    bool PasswordProtectedSecret::setPassword(const string& pw)
    {
      // this function is only intended for
      // cases if there is no previously set password
      if (!(pwClear.empty())) return false;

      if (pw.empty()) return false;

      password2SymKey(pw);

      // if we have content, try to decode it to check
      // the password validity
      if (cipher.notEmpty())
      {
        try
        {
          getSecret(SodiumSecureMemType::Normal);
        }
        catch (WrongPassword)
        {
          pwClear.releaseMemory();
          return false;
        }
      }

      return true;
    }

    //----------------------------------------------------------------------------

    bool PasswordProtectedSecret::isValidPassword(const string& pw)
    {
      if (pwClear.empty())
      {
        throw NoPasswordSet{};
      }

      pwClear.setAccess(SodiumSecureMemAccess::RO);
      bool isEqual = lib->memcmp(pwClear.toMemView(), MemView{pw});
      pwClear.setAccess(SodiumSecureMemAccess::NoAccess);

      return isEqual;
    }

    //----------------------------------------------------------------------------

    string PasswordProtectedSecret::asString(bool useBase64) const
    {
      Net::OutMessage msg;

      msg.addByte(static_cast<uint8_t>(hashConfig.algo));
      msg.addUI64(hashConfig.memlimit);
      msg.addUI64(hashConfig.opslimit);
      msg.addMemView(hashConfig.salt.toMemView());  // also works for an empty salt
      msg.addMemView(cipher.view());  // also works for an empty cipher
      msg.addMemView(nonce.toMemView());

      // not very efficient... hmmmm...
      MemView mv{msg.view()};
      string rawData{mv.to_charPtr(), mv.size()};

      return useBase64 ? toBase64(rawData) : rawData;
    }

    //----------------------------------------------------------------------------

    void PasswordProtectedSecret::password2SymKey(const string& pw)
    {
      // convert the pw into a hash; the hash will be used as the secret key for
      // encrypting the secret
      SodiumLib::SecretBoxKey sk;
      sk = lib->pwhash(MemView{pw}, sk.size(), hashConfig, sk.getType());
      if (sk.empty())
      {
        throw PasswordHashingError{};
      }

      symKey.setAccess(SodiumSecureMemAccess::RW);
      symKey = std::move(sk);
      symKey.setAccess(SodiumSecureMemAccess::NoAccess);

      if (!(pwClear.empty())) pwClear.setAccess(SodiumSecureMemAccess::RW);
      pwClear = SodiumSecureMemory{pw, SodiumSecureMemType::Locked};
      pwClear.setAccess(SodiumSecureMemAccess::NoAccess);
    }

    //----------------------------------------------------------------------------

    int SodiumBase64Enconding2int(const SodiumBase64Enconding& enc)
    {
      switch (enc)
      {
      case SodiumBase64Enconding::Original:
        return sodium_base64_VARIANT_ORIGINAL;

      case SodiumBase64Enconding::Original_NoPadding:
        return sodium_base64_VARIANT_ORIGINAL_NO_PADDING;

      case SodiumBase64Enconding::URLSafe:
        return sodium_base64_VARIANT_URLSAFE;

      case SodiumBase64Enconding::URLSafe_NoPadding:
        return sodium_base64_VARIANT_URLSAFE_NO_PADDING;
      }

      return sodium_base64_VARIANT_ORIGINAL;
    }

    //----------------------------------------------------------------------------

    DiffieHellmannExchanger2::DiffieHellmannExchanger2(bool _isClient)
      :isClient{_isClient}, lib{SodiumLib::getInstance()}
    {
      if (lib == nullptr)
      {
        throw SodiumNotAvailableException{};
      }

      tie(sk, pk) = lib->genKeyExchangeKeyPair();
      sk.setAccess(SodiumSecureMemAccess::NoAccess);
    }

    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------
    //----------------------------------------------------------------------------

    DiffieHellmannExchanger2::DiffieHellmannExchanger2(bool _isClient, const string& seed_B64)
      :isClient{_isClient}, lib{SodiumLib::getInstance()}
    {
      if (lib == nullptr)
      {
        throw SodiumNotAvailableException{};
      }

      // convert the base64 data into binary data and
      // initialize the seed
      bool isOkay{false};
      SodiumLib::KX_KeySeed seed;
      try
      {
        string binSeed = lib->base642Bin(seed_B64);
        isOkay = seed.fillFromString(binSeed);
      }
      catch (...)
      {
        throw SodiumInvalidKey("DiffieHellmannExchanger2 ctor");
      }

      if (!isOkay)
      {
        throw SodiumInvalidKey("DiffieHellmannExchanger2 ctor");
      }

      // do the actual key generation
      tie(sk, pk) = lib->genKeyExchangeKeyPair(seed);
      sk.setAccess(SodiumSecureMemAccess::NoAccess);
    }

    //----------------------------------------------------------------------------

    SodiumLib::KX_PublicKey DiffieHellmannExchanger2::getMyPublicKey()
    {
      return SodiumLib::KX_PublicKey{pk};
    }

    //----------------------------------------------------------------------------

    pair<SodiumLib::KX_SessionKey, SodiumLib::KX_SessionKey> DiffieHellmannExchanger2::getSessionKeys(const SodiumLib::KX_PublicKey& othersPublicKey)
    {
      sk.setAccess(SodiumSecureMemAccess::RO);
      auto result = isClient ?
            lib->getClientSessionKeys(pk, sk, othersPublicKey) :
            lib->getServerSessionKeys(pk, sk, othersPublicKey);
      sk.setAccess(SodiumSecureMemAccess::NoAccess);

      return result;
    }

    //----------------------------------------------------------------------------

  }
}
