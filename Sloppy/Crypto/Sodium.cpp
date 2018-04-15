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
#include "../Net/Net.h"
#include "Crypto.h"

namespace Sloppy
{
  namespace Crypto
  {
    // allocate memory for the static instance pointer
    unique_ptr<SodiumLib> SodiumLib::inst;

    //----------------------------------------------------------------------------

    SodiumSecureMemory::SodiumSecureMemory(size_t _len, SodiumSecureMemType t)
      :rawPtr{nullptr}, nBytes{_len}, type{t}, lib{SodiumLib::getInstance()},
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

    SodiumSecureMemory SodiumSecureMemory::copy() const
    {
      return asCopy(*this);
    }

    //----------------------------------------------------------------------------

    bool SodiumSecureMemory::operator ==(const SodiumSecureMemory& other) const
    {
      if ((this->rawPtr == other.rawPtr) && (this->nBytes == other.nBytes)) return true;   // identity

      // we actually have to compare memory contents
      return lib->memcmp(this->toMemView(), other.toMemView());
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumSecureMemory::asCopy(const SodiumSecureMemory& src)
    {
      if (!(src.canRead()))
      {
        throw SodiumMemoryGuardException{"creating deep SodiumSecureMemory copy"};
      }

      // allocate the same amount and type of memory
      //
      // this might throw exceptions if we're running out of memory
      SodiumSecureMemory cpy{src.size(), src.getType()};

      SodiumSecureMemAccess oldProtection = src.getProtection();

      // do the copy itself
      memcpy(cpy.rawPtr, src.rawPtr, src.size());

      // re-lock the source
      if ((src.getType() == SodiumSecureMemType::Guarded) && (oldProtection != SodiumSecureMemAccess::RW))
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



    //----------------------------------------------------------------------------

    SodiumLib*SodiumLib::getInstance()
    {
      if (inst == nullptr)
      {
        // clear all old dynamic loader errors
        dlerror();

        // try to load the library; use different names for
        // Windows and Linux
        void *handle = nullptr;
        for (const string& libName : {"libsodium.so", "libsodium-18.dll"})
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
      *(void **)(&(sodium.init)) = dlsym(libHandle, "sodium_init");
      *(void **)(&(sodium.bin2hex)) = dlsym(libHandle, "sodium_bin2hex");
      *(void **)(&(sodium.hex2bin)) = dlsym(libHandle, "sodium_hex2bin");
      *(void **)(&(sodium.bin2base64)) = dlsym(libHandle, "sodium_bin2base64");
      *(void **)(&(sodium.base642bin)) = dlsym(libHandle, "sodium_base642bin");
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
      *(void **)(&(sodium.crypto_generichash)) = dlsym(libHandle, "crypto_generichash");
      *(void **)(&(sodium.crypto_generichash_init)) = dlsym(libHandle, "crypto_generichash_init");
      *(void **)(&(sodium.crypto_generichash_update)) = dlsym(libHandle, "crypto_generichash_update");
      *(void **)(&(sodium.crypto_generichash_final)) = dlsym(libHandle, "crypto_generichash_final");
      *(void **)(&(sodium.crypto_generichash_statebytes)) = dlsym(libHandle, "crypto_generichash_statebytes");
      *(void **)(&(sodium.crypto_shorthash)) = dlsym(libHandle, "crypto_shorthash");
      *(void **)(&(sodium.crypto_pwhash)) = dlsym(libHandle, "crypto_pwhash");
      *(void **)(&(sodium.crypto_pwhash_str)) = dlsym(libHandle, "crypto_pwhash_str");
      *(void **)(&(sodium.crypto_pwhash_str_verify)) = dlsym(libHandle, "crypto_pwhash_str_verify");
      *(void **)(&(sodium.crypto_pwhash_scryptsalsa208sha256)) = dlsym(libHandle, "crypto_pwhash_scryptsalsa208sha256");
      *(void **)(&(sodium.crypto_pwhash_scryptsalsa208sha256_str)) = dlsym(libHandle, "crypto_pwhash_scryptsalsa208sha256_str");
      *(void **)(&(sodium.crypto_pwhash_scryptsalsa208sha256_str_verify)) = dlsym(libHandle, "crypto_pwhash_scryptsalsa208sha256_str_verify");
      *(void **)(&(sodium.crypto_scalarmult)) = dlsym(libHandle, "crypto_scalarmult");
      // *(void **)(&(sodium.)) = dlsym(libHandle, "sodium_");

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
          (sodium.crypto_scalarmult == nullptr)
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
      sodium.bin2hex((char *)result.c_str(), resultSize + 1, (unsigned char*)(binData.c_str()), binData.size());

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

      // calculate the number of output characters
      size_t outLen = sodium_base64_ENCODED_LEN(bin.size(), variant);

      // allocate sufficient memory
      string out;
      out.resize(outLen);

      // do the conversion
      char * rc = sodium.bin2base64((char *) out.c_str(), outLen,
                        (const unsigned char*) bin.c_str(), bin.size(),
                        variant);

      if (rc == nullptr)
      {
        throw SodiumConversionError{"bin2base64 encoding (string --> string)"};
      }

      return out;
    }

    //----------------------------------------------------------------------------

    MemArray SodiumLib::bin2Base64(const MemView& bin, SodiumBase64Enconding enc) const
    {
      if (bin.empty()) return MemArray{};

      // determine the encoding variant
      int variant = SodiumBase64Enconding2int(enc);

      // calculate the number of output characters
      size_t outLen = sodium_base64_ENCODED_LEN(bin.size(), variant);

      // allocate sufficient memory
      MemArray out{outLen};

      // do the conversion
      char * rc = sodium.bin2base64(out.to_charPtr(), outLen,
                        bin.to_ucPtr(), bin.size(),
                        variant);

      if (rc == nullptr)
      {
        throw SodiumConversionError{"bin2base64 encoding (MemView --> MemArray)"};
      }

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

    MemArray SodiumLib::crypto_secretbox_easy(const MemView& msg, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key)
    {
      // the message and the other parameters should be valid
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_secretbox_easy");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidKey("crypto_secretbox_easy");
      }
      if (key.empty())
      {
        throw SodiumInvalidNonce("crypto_secretbox_easy");
      }

      // allocate space for the result
      MemArray cipher{crypto_secretbox_MACBYTES + msg.size()};

      // do the actual encryption
      sodium.crypto_secretbox_easy(cipher.to_ucPtr(), msg.to_ucPtr(), msg.size(), nonce.to_ucPtr_ro(), key.to_ucPtr_ro());

      // return the result
      return cipher;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::crypto_secretbox_open_easy__internal(const MemArray& targetBuf, const MemView& cipher, const SodiumLib::SecretBoxNonceType& nonce, const SodiumLib::SecretBoxKeyType& key)
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

    MemArray SodiumLib::crypto_secretbox_open_easy(const MemView& cipher, const SodiumLib::SecretBoxNonceType& nonce, const SodiumLib::SecretBoxKeyType& key)
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
      bool isOkay = crypto_secretbox_open_easy__internal(msg, cipher, nonce, key);

      // return the clear text or an invalid buffer
      return isOkay ? std::move(msg) : MemArray{};
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::crypto_secretbox_open_easy__secure(const MemView& cipher, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key, SodiumSecureMemType clearTextProtection)
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
      bool isOkay = crypto_secretbox_open_easy__internal(msg.toNotOwningArray(), cipher, nonce, key);

      // return the clear text or an invalid buffer
      return isOkay ? std::move(msg) : SodiumSecureMemory{};
    }

    //----------------------------------------------------------------------------

    pair<MemArray, SodiumLib::SecretBoxMacType> SodiumLib::crypto_secretbox_detached(const MemView& msg, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key)
    {
      // the message and the other parameters should be valid
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_secretbox_easy");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidKey("crypto_secretbox_easy");
      }
      if (key.empty())
      {
        throw SodiumInvalidNonce("crypto_secretbox_easy");
      }

      // allocate space for the result
      MemArray cipher{msg.size()};
      SodiumLib::SecretBoxMacType mac;

      // do the actual encryption
      sodium.crypto_secretbox_detached(cipher.to_ucPtr(), mac.to_ucPtr_rw(), msg.to_ucPtr(), msg.size(), nonce.to_ucPtr_ro(), key.to_ucPtr_ro());

      // return the result
      return make_pair(std::move(cipher), std::move(mac));
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::crypto_secretbox_open_detached(const MemView& cipher, const SecretBoxMacType& mac, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key, SodiumSecureMemType clearTextProtection)
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

    string SodiumLib::crypto_secretbox_easy(const string& msg, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key)
    {
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_secretbox_easy");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidKey("crypto_secretbox_easy");
      }
      if (key.empty())
      {
        throw SodiumInvalidNonce("crypto_secretbox_easy");
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

    string SodiumLib::crypto_secretbox_open_easy(const string& cipher, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key)
    {
      if (nonce.empty())
      {
        throw SodiumInvalidKey("crypto_secretbox_open_easy");
      }
      if (key.empty())
      {
        throw SodiumInvalidNonce("crypto_secretbox_open_easy");
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
                                                     (const unsigned char *)cipher.c_str(), cipher.size(),
                                                     nonce.to_ucPtr_ro(), key.to_ucPtr_ro());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? msg : string{};
    }

    //----------------------------------------------------------------------------

    pair<string, SodiumLib::SecretBoxMacType> SodiumLib::crypto_secretbox_detached(const string& msg, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key)
    {
      // the message and the other parameters should be valid
      if (msg.empty())
      {
        throw SodiumInvalidMessage("crypto_secretbox_easy");
      }
      if (nonce.empty())
      {
        throw SodiumInvalidKey("crypto_secretbox_easy");
      }
      if (key.empty())
      {
        throw SodiumInvalidNonce("crypto_secretbox_easy");
      }

      // allocate space for the result
      string cipher;
      cipher.resize(msg.size());
      SecretBoxMacType mac;

      // do the actual encryption
      sodium.crypto_secretbox_detached((unsigned char *)cipher.c_str(), mac.to_ucPtr_rw(),
                                       (const unsigned char *)msg.c_str(), msg.size(),
                                       nonce.to_ucPtr_ro(), key.to_ucPtr_ro());

      // return the result
      return make_pair(cipher, mac);
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_secretbox_open_detached(const string& cipher, const SecretBoxMacType& mac, const SecretBoxNonceType& nonce, const SecretBoxKeyType& key)
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
                                                         (const unsigned char *)cipher.c_str(),
                                                         mac.to_ucPtr_ro(), cipher.size(),
                                                         nonce.to_ucPtr_ro(), key.to_ucPtr_ro());

      // return the clear text or an invalid buffer
      return (isOkay == 0) ? msg  : string{};
    }

    //----------------------------------------------------------------------------

    SodiumLib::AuthTagType SodiumLib::crypto_auth(const MemView& msg, const SodiumLib::AuthKeyType& key)
    {
      if (!(msg.isValid())) return AuthTagType{};
      if (!(key.isValid())) return AuthTagType{};

      AuthTagType result;
      sodium.crypto_auth(result.get_uc(), msg.get_uc(), msg.getSize(), key.get_uc());

      return result;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::crypto_auth_verify(const MemView& msg, const SodiumLib::AuthTagType& tag, const SodiumLib::AuthKeyType& key)
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
                                                 const MemView& msg, const MemView& nonce, const MemView& key, const ManagedBuffer& ad)
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

    SodiumSecureMemory SodiumLib::crypto_aead_decrypt(int (*funcPtr)(unsigned char*, unsigned long long*, unsigned char*, const unsigned char*, unsigned long long, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*), size_t tagSize, const MemView& cipher, const MemView& nonce, const MemView& key, const ManagedBuffer& ad, SodiumSecureMemType clearTextProtection)
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

    string SodiumLib::crypto_aead_encrypt(int (*funcPtr)(unsigned char*, unsigned long long*, const unsigned char*, unsigned long long, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*, const unsigned char*), size_t tagSize, const string& msg, const MemView& nonce, const MemView& key, const string& ad)
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

    string SodiumLib::crypto_aead_decrypt(int (*funcPtr)(unsigned char*, unsigned long long*, unsigned char*, const unsigned char*, unsigned long long, const unsigned char*, unsigned long long, const unsigned char*, const unsigned char*), size_t tagSize, const string& cipher, const MemView& nonce, const MemView& key, const string& ad)
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

    pair<unsigned long long, size_t> SodiumLib::pwHashConfigToValues(SodiumLib::PasswdHashStrength strength, SodiumLib::PasswdHashAlgo algo)
    {
      // strength "Moderate" is not defined for Scrypt
      if ((algo == PasswdHashAlgo::Scrypt) && (strength == PasswdHashStrength::Moderate)) return make_pair(0,0);

      unsigned long long opslimit;
      size_t memlimit;
      if (algo == PasswdHashAlgo::Argon2)
      {
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
      } else {
        opslimit = (strength == PasswdHashStrength::Interactive) ? crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_INTERACTIVE : crypto_pwhash_scryptsalsa208sha256_OPSLIMIT_SENSITIVE;
        memlimit = (strength == PasswdHashStrength::Interactive) ? crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_INTERACTIVE : crypto_pwhash_scryptsalsa208sha256_MEMLIMIT_SENSITIVE;
      }

      return make_pair(opslimit, memlimit);
    }

    //----------------------------------------------------------------------------

    ManagedBuffer SodiumLib::crypto_aead_chacha20poly1305_encrypt(const MemView& msg,
                                                                  const SodiumLib::AEAD_ChaCha20Poly1305_NonceType& nonce, const SodiumLib::AEAD_ChaCha20Poly1305_KeyType& key,
                                                                  const ManagedBuffer& ad)
    {
      return crypto_aead_encrypt(sodium.crypto_aead_chacha20poly1305_encrypt, crypto_aead_chacha20poly1305_ABYTES, msg, nonce, key, ad);
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::crypto_aead_chacha20poly1305_decrypt(const MemView& cipher, const SodiumLib::AEAD_ChaCha20Poly1305_NonceType& nonce,
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

    ManagedBuffer SodiumLib::crypto_aead_aes256gcm_encrypt(const MemView& msg, const SodiumLib::AEAD_AES256GCM_NonceType& nonce, const SodiumLib::AEAD_AES256GCM_KeyType& key, const ManagedBuffer& ad)
    {
      if (sodium.crypto_aead_aes256gcm_is_available() != 1) return ManagedBuffer{};

      return crypto_aead_encrypt(sodium.crypto_aead_aes256gcm_encrypt, crypto_aead_aes256gcm_ABYTES, msg, nonce, key, ad);
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::crypto_aead_aes256gcm_decrypt(const MemView& cipher, const SodiumLib::AEAD_AES256GCM_NonceType& nonce, const SodiumLib::AEAD_AES256GCM_KeyType& key, const ManagedBuffer& ad, SodiumSecureMemType clearTextProtection)
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

    bool SodiumLib::genAsymCryptoKeyPair(AsymCrypto_PublicKey& pk_out, AsymCrypto_SecretKey& sk_out)
    {
      if (!(pk_out.isValid())) return false;
      if (!(sk_out.isValid())) return false;

      sodium.crypto_box_keypair(pk_out.get_uc(), sk_out.get_uc());
      return true;
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

    ManagedBuffer SodiumLib::crypto_box_easy(const MemView& msg, const SodiumLib::AsymCrypto_Nonce& nonce,
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

    SodiumSecureMemory SodiumLib::crypto_box_open_easy(const MemView& cipher, const SodiumLib::AsymCrypto_Nonce& nonce,
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

    pair<ManagedBuffer, SodiumLib::AsymCrypto_Tag> SodiumLib::crypto_box_detached(const MemView& msg, const SodiumLib::AsymCrypto_Nonce& nonce,
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

    SodiumSecureMemory SodiumLib::crypto_box_open_detached(const MemView& cipher, const SodiumLib::AsymCrypto_Tag& mac, const SodiumLib::AsymCrypto_Nonce& nonce,
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

    ManagedBuffer SodiumLib::crypto_sign(const MemView& msg, const SodiumLib::AsymSign_SecretKey& sk)
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

    ManagedBuffer SodiumLib::crypto_sign_open(const MemView& signedMsg, const SodiumLib::AsymSign_PublicKey& pk)
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

    bool SodiumLib::crypto_sign_detached(const MemView& msg, const SodiumLib::AsymSign_SecretKey& sk, SodiumLib::AsymSign_Signature& sig_out)
    {
      // check input validity
      if (!(msg.isValid())) return false;
      if (!(sk.isValid())) return false;

      // sign
      sodium.crypto_sign_detached(sig_out.get_uc(), nullptr, msg.get_uc(), msg.getSize(), sk.get_uc());

      return true;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::crypto_sign_verify_detached(const MemView& msg, const AsymSign_Signature& sig, const SodiumLib::AsymSign_PublicKey& pk)
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

    ManagedBuffer SodiumLib::crypto_generichash(const MemView& inData)
    {
      if (!(inData.isValid())) return ManagedBuffer{};

      // allocate space for the result
      ManagedBuffer hash{crypto_generichash_BYTES};

      // calculate the hash
      sodium.crypto_generichash(hash.get_uc(), crypto_generichash_BYTES, inData.get_uc(), inData.getSize(), nullptr, 0);

      return hash;
    }

    //----------------------------------------------------------------------------

    ManagedBuffer SodiumLib::crypto_generichash(const MemView& inData, const SodiumLib::GenericHashKey& key)
    {
      if (!(inData.isValid())) return ManagedBuffer{};
      if (!(key.isValid())) return ManagedBuffer{};

      // allocate space for the result
      ManagedBuffer hash{crypto_generichash_BYTES};

      // calculate the hash
      sodium.crypto_generichash(hash.get_uc(), crypto_generichash_BYTES, inData.get_uc(), inData.getSize(),
                                key.get_uc(), key.getSize());

      return hash;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_generichash(const string& inData)
    {
      if (inData.empty()) return string{};

      string hash;
      hash.resize(crypto_generichash_BYTES);

      sodium.crypto_generichash((unsigned char*)hash.c_str(), crypto_generichash_BYTES,
                                (const unsigned char*)inData.c_str(), inData.size(), nullptr, 0);

      return hash;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_generichash(const string& inData, const SodiumLib::GenericHashKey& key)
    {
      if (inData.empty()) return string{};
      if (!(key.isValid())) return string{};

      string hash;
      hash.resize(crypto_generichash_BYTES);

      sodium.crypto_generichash((unsigned char*)hash.c_str(), crypto_generichash_BYTES, (const unsigned char*)inData.c_str(), inData.size(),
                                key.get_uc(), key.getSize());

      return hash;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::crypto_generichash_init(crypto_generichash_state* state)
    {
      if (state == nullptr) return false;

      sodium.crypto_generichash_init(state, nullptr, 0, crypto_generichash_BYTES);

      return true;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::crypto_generichash_init(crypto_generichash_state* state, const GenericHashKey& k)
    {
      if (state == nullptr) return false;
      if (!(k.isValid())) return false;

      sodium.crypto_generichash_init(state, k.get_uc(), k.getSize(), crypto_generichash_BYTES);

      return true;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::crypto_generichash_update(crypto_generichash_state* state, const MemView& inData)
    {
      if (state == nullptr) return false;
      if (!(inData.isValid())) return false;

      sodium.crypto_generichash_update(state, inData.get_uc(), inData.getSize());
      return true;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::crypto_generichash_update(crypto_generichash_state* state, const string& inData)
    {
      if (state == nullptr) return false;
      if (inData.empty()) return false;

      sodium.crypto_generichash_update(state, (const unsigned char*)inData.c_str(), inData.size());
      return true;
    }

    //----------------------------------------------------------------------------

    ManagedBuffer SodiumLib::crypto_generichash_final(crypto_generichash_state* state)
    {
      if (state == nullptr) return ManagedBuffer{};

      // allocate space for the result
      ManagedBuffer hash{crypto_generichash_BYTES};

      // finalize and get the hash
      sodium.crypto_generichash_final(state, hash.get_uc(), crypto_generichash_BYTES);

      return hash;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_generichash_final_string(crypto_generichash_state* state)
    {
      if (state == nullptr) return string{};

      // allocate space for the result
      string hash;
      hash.resize(crypto_generichash_BYTES);

      // finalize and get the hash
      sodium.crypto_generichash_final(state, (unsigned char*)hash.c_str(), crypto_generichash_BYTES);

      return hash;
    }

    //----------------------------------------------------------------------------

    ManagedBuffer SodiumLib::crypto_shorthash(const MemView& inData, const SodiumLib::ShorthashKey& k)
    {
      if (!(inData.isValid())) return ManagedBuffer{};
      if (!(k.isValid())) return ManagedBuffer{};

      // allocate space for the result
      ManagedBuffer hash{crypto_shorthash_BYTES};

      // get the hash
      sodium.crypto_shorthash(hash.get_uc(), inData.get_uc(), inData.getSize(), k.get_uc());

      return hash;
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_shorthash(const string& inData, const SodiumLib::ShorthashKey& k)
    {
      if (inData.empty()) return string{};
      if (!(k.isValid())) return string{};

      // allocate space for the result
      string hash;
      hash.resize(crypto_shorthash_BYTES);

      // get the hash
      sodium.crypto_shorthash((unsigned char*)hash.c_str(), (const unsigned char*)inData.c_str(), inData.size(), k.get_uc());

      return hash;
    }

    //----------------------------------------------------------------------------

    pair<SodiumSecureMemory, SodiumLib::PwHashData> SodiumLib::crypto_pwhash(const MemView& pw, size_t hashLen, SodiumLib::PasswdHashStrength strength,
                                                                     SodiumLib::PasswdHashAlgo algo, SodiumSecureMemType memType)
    {
      auto errorResult = make_pair(SodiumSecureMemory{}, PwHashData{});

      // construct a new PwHashData struct with the hashing data.
      // the salt is empty and will thus be filled with random data
      PwHashData hDat{};
      hDat.algo = algo;

      // determine opslimit and memlimit
      tie(hDat.opslimit, hDat.memlimit) = pwHashConfigToValues(strength, algo);

      // strength "Moderate" is not defined for Scrypt, this is indicated
      // by pwHashConfigToValues by returning two zeros
      if ((hDat.opslimit == 0) && (hDat.memlimit == 0)) return errorResult;

      // call the hash function
      SodiumSecureMemory hash = crypto_pwhash(pw, hashLen, hDat, memType);

      return hash.isValid() ? make_pair(std::move(hash), std::move(hDat)) : std::move(errorResult);
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumLib::crypto_pwhash(const MemView& pw, size_t hashLen, PwHashData& hDat, SodiumSecureMemType memType)
    {
      if (!(pw.isValid())) return SodiumSecureMemory{};
      if (hashLen == 0) return SodiumSecureMemory{};

      // determine the salt length
      size_t saltLen = (hDat.algo == PasswdHashAlgo::Argon2) ? crypto_pwhash_SALTBYTES : crypto_pwhash_scryptsalsa208sha256_SALTBYTES;

      // if the salt has the wrong length, return with error
      if ((hDat.salt.isValid()) && (hDat.salt.getSize() != saltLen)) return SodiumSecureMemory{};

      // if the salt is empty create a new, random one
      // and store it in the user provided struct
      if (!(hDat.salt.isValid()))
      {
        ManagedBuffer salt{saltLen};
        randombytes_buf(salt);
        hDat.salt = std::move(salt);
      }

      // allocate the result buffer
      SodiumSecureMemory hash{hashLen, memType};

      // call the hash function
      int rc = -1;
      if (hDat.algo == PasswdHashAlgo::Argon2)
      {
        rc = sodium.crypto_pwhash(hash.get_uc(), hash.getSize(), pw.get_c(), pw.getSize(),
                                  hDat.salt.get_uc(), hDat.opslimit, hDat.memlimit, crypto_pwhash_ALG_DEFAULT);
      } else {
        rc = sodium.crypto_pwhash_scryptsalsa208sha256(hash.get_uc(), hash.getSize(), pw.get_c(), pw.getSize(),
                                  hDat.salt.get_uc(), hDat.opslimit, hDat.memlimit);
      }

      return (rc == 0) ? std::move(hash) : SodiumSecureMemory{};
    }

    //----------------------------------------------------------------------------

    pair<string, string> SodiumLib::crypto_pwhash(const string& pw, size_t hashLen, SodiumLib::PasswdHashStrength strength,
                                                  SodiumLib::PasswdHashAlgo algo, SodiumSecureMemType memType)
    {
      // since password, hash and salt will be usually rather short
      // we go for copy operations here instead of repeating the whole
      // wrapper code again
      ManagedBuffer _pw{pw};
      auto tmp = crypto_pwhash(_pw, hashLen, strength, algo, memType);
      SodiumSecureMemory hash;
      PwHashData hDat;
      hash = std::move(tmp.first);
      hDat = std::move(tmp.second);
      if (hash.isValid() && hDat.salt.isValid())
      {
        return make_pair(hash.copyToString(), hDat.salt.copyToString());
      }
      return make_pair(string{}, string{});
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_pwhash_str(const MemView& pw, SodiumLib::PasswdHashStrength strength, SodiumLib::PasswdHashAlgo algo)
    {
      if (!(pw.isValid())) return string{};

      unsigned long long opslimit;
      size_t memlimit;
      tie(opslimit, memlimit) = pwHashConfigToValues(strength, algo);
      if ((opslimit == 0) && (memlimit == 0)) return string{};

      string hash;
      int rc = -1;
      if (algo == PasswdHashAlgo::Argon2)
      {
        char s[crypto_pwhash_STRBYTES];
        rc = sodium.crypto_pwhash_str(s, pw.get_c(), pw.getSize(), opslimit, memlimit);
        hash = string{s};
      } else {
        char s[crypto_pwhash_scryptsalsa208sha256_STRBYTES];
        rc = sodium.crypto_pwhash_scryptsalsa208sha256_str(s, pw.get_c(), pw.getSize(), opslimit, memlimit);
        hash = string{s};
      }

      return (rc == 0) ? hash : string{};
    }

    //----------------------------------------------------------------------------

    string SodiumLib::crypto_pwhash_str(const string& pw, SodiumLib::PasswdHashStrength strength, SodiumLib::PasswdHashAlgo algo)
    {
      // since the password will be usually rather short
      // we go for copy operations here instead of repeating the whole
      // wrapper code again
      ManagedBuffer _pw{pw};
      return crypto_pwhash_str(_pw, strength, algo);
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::crypto_pwhash_str_verify(const MemView& pw, const string& hashResult, PasswdHashAlgo algo)
    {
      int rc = -1;
      if (algo == PasswdHashAlgo::Argon2)
      {
        rc = sodium.crypto_pwhash_str_verify(hashResult.c_str(), pw.get_c(), pw.getSize());
      } else {
        rc = sodium.crypto_pwhash_scryptsalsa208sha256_str_verify(hashResult.c_str(), pw.get_c(), pw.getSize());
      }

      return (rc == 0);
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::crypto_pwhash_str_verify(const string& pw, const string& hashResult, SodiumLib::PasswdHashAlgo algo)
    {
      // since the password will be usually rather short
      // we go for copy operations here instead of repeating the whole
      // wrapper code again
      ManagedBuffer _pw{pw};
      return crypto_pwhash_str_verify(_pw, hashResult, algo);
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::genDHKeyPair(SodiumLib::DH_PublicKey& pk_out, SodiumLib::DH_SecretKey& sk_out)
    {
      if (!(sk_out.isValid())) return false;
      randombytes_buf(sk_out);

      genPublicDHKeyFromSecretKey(sk_out, pk_out);
      return true;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::genDHSharedSecret(const SodiumLib::DH_SecretKey& mySecretKey, const SodiumLib::DH_PublicKey& othersPublicKey, SodiumLib::DH_SharedSecret& sh_out)
    {
      if (!(mySecretKey.isValid())) return false;
      if (!(othersPublicKey.isValid())) return false;
      if (!(sh_out.isValid())) return false;

      sodium.crypto_scalarmult(sh_out.get_uc(), mySecretKey.get_uc(), othersPublicKey.get_uc());
      return true;
    }

    //----------------------------------------------------------------------------

    bool SodiumLib::genPublicDHKeyFromSecretKey(const SodiumLib::DH_SecretKey& sk, SodiumLib::DH_PublicKey& pk_out)
    {
      if (!(sk.isValid())) return false;

      sodium.crypto_scalarmult_base(pk_out.get_uc(), sk.get_uc());
      return true;
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

    ManagedBuffer SodiumSecretBox::encryptCombined(const MemView& msg)
    {
      setKeyLockState(false);
      auto result = lib->crypto_secretbox_easy(msg, nextNonce, key);
      setKeyLockState(true);

      if (autoIncrementNonce) incrementNonce();

      return result;
    }

    //----------------------------------------------------------------------------

    pair<ManagedBuffer, ManagedBuffer> SodiumSecretBox::encryptDetached(const MemView& msg)
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

    SodiumSecureMemory SodiumSecretBox::decryptCombined(const MemView& cipher, SodiumSecureMemType clearTextProtection)
    {
      setKeyLockState(false);
      SodiumSecureMemory result = lib->crypto_secretbox_open_easy__secure(cipher, nextNonce, key, clearTextProtection);
      setKeyLockState(true);

      if (autoIncrementNonce) incrementNonce();

      return result;
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory SodiumSecretBox::decryptDetached(const MemView& cipher, const MemView& mac, SodiumSecureMemType clearTextProtection)
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

    GenericHasher::GenericHasher()
      :isFinalized{false}, lib{SodiumLib::getInstance()}
    {
      if (lib == nullptr)
      {
        throw SodiumNotAvailableException{};
      }

      lib->crypto_generichash_init(&state);
    }

    //----------------------------------------------------------------------------

    GenericHasher::GenericHasher(const SodiumLib::GenericHashKey& k)
      :isFinalized{false}, lib{SodiumLib::getInstance()}
    {
      if (lib == nullptr)
      {
        throw SodiumNotAvailableException{};
      }

      lib->crypto_generichash_init(&state, k);
    }

    //----------------------------------------------------------------------------

    bool GenericHasher::append(const MemView& inData)
    {
      if (isFinalized) return false;

      return lib->crypto_generichash_update(&state, inData);
    }

    //----------------------------------------------------------------------------

    bool GenericHasher::append(const string& inData)
    {
      if (isFinalized) return false;

      return lib->crypto_generichash_update(&state, inData);
    }

    //----------------------------------------------------------------------------

    ManagedBuffer GenericHasher::finalize()
    {
      if (isFinalized) return ManagedBuffer{};

      auto hash = lib->crypto_generichash_final(&state);
      isFinalized = true;

      return hash;
    }

    //----------------------------------------------------------------------------

    string GenericHasher::finalize_string()
    {
      if (isFinalized) return string{};

      auto hash = lib->crypto_generichash_final_string(&state);
      isFinalized = true;

      return hash;
    }

    //----------------------------------------------------------------------------

    DiffieHellmannExchanger::DiffieHellmannExchanger(bool _isClient)
      :isClient{_isClient}, lib{SodiumLib::getInstance()}
    {
      if (lib == nullptr)
      {
        throw SodiumNotAvailableException{};
      }

      lib->genDHKeyPair(pk, sk);
      sk.setAccess(SodiumSecureMemAccess::NoAccess);
    }

    //----------------------------------------------------------------------------

    SodiumLib::DH_PublicKey DiffieHellmannExchanger::getMyPublicKey()
    {
      return SodiumLib::DH_PublicKey::asCopy(pk);
    }

    //----------------------------------------------------------------------------

    DiffieHellmannExchanger::SharedSecret DiffieHellmannExchanger::getSharedSecret(const SodiumLib::DH_PublicKey& othersPublicKey)
    {
      SodiumLib::DH_SharedSecret shared;

      sk.setAccess(SodiumSecureMemAccess::RO);
      bool isOkay = lib->genDHSharedSecret(sk, othersPublicKey, shared);
      sk.setAccess(SodiumSecureMemAccess::NoAccess);

      if (!isOkay)
      {
        throw runtime_error("Damn, couldn't calculate Diffie-Hellmann shared secret. FIX: add a special exception for this!");
      }

      // hash this result with the two public keys
      GenericHasher hasher;
      hasher.append(shared);
      if (isClient)
      {
        hasher.append(pk);
        hasher.append(othersPublicKey);
      } else {
        hasher.append(othersPublicKey);
        hasher.append(pk);
      }

      SharedSecret result;
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
      tie(hashConfig.opslimit, hashConfig.memlimit) = lib->pwHashConfigToValues(pwStrength, pwAlgo);
      hashConfig.algo = pwAlgo;
      lib->randombytes_buf(hashConfig.salt);

      // set an initial nonce although this will never be used.
      // but if don't do this now, a subsequent call to asString()
      // would produce a malformed string
      lib->randombytes_buf(nonce);
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
        Net::MessageDissector md{rawData};

        // determine the algo type
        uint8_t algoId = md.getByte();
        if (algoId == 0)
        {
          hashConfig.algo = SodiumLib::PasswdHashAlgo::Argon2;
        } else {
          hashConfig.algo = SodiumLib::PasswdHashAlgo::Scrypt;
        }

        // get memlimit and opslimit
        static_assert(sizeof(unsigned long long) == sizeof(size_t), "Invalid size of unsigned long long!");
        hashConfig.memlimit = md.getUI64();
        hashConfig.opslimit = md.getUI64();

        // read the salt
        hashConfig.salt = md.getManagedBuffer();

        // what remains are the cipher and nonce
        cipher = md.getManagedBuffer();

        ManagedBuffer _nonce = md.getManagedBuffer();
        if (!(nonce.fillFromMemView(_nonce)))
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
      if (!(sec.isValid()))
      {
        cipher.releaseMemory();
        return true;
      }

      if (!(pwClear.isValid())) return false;

      // prepare a fresh random nonce for
      // each symmetric encryption step
      lib->randombytes_buf(nonce);

      // do the encryption
      symKey.setAccess(SodiumSecureMemAccess::RO);
      ManagedBuffer _cipher = lib->crypto_secretbox_easy(sec, nonce, symKey);
      symKey.setAccess(SodiumSecureMemAccess::NoAccess);

      // only store the result if the encryption was successful
      if (!(_cipher.isValid())) return false;
      cipher = std::move(_cipher);

      return true;
    }

    //----------------------------------------------------------------------------

    bool PasswordProtectedSecret::setSecret(const string& sec)
    {
      // we are lazy and copy the input data over into a
      // managed memory. this consumes more memory and time but we avoid
      // some typing
      ManagedBuffer tmp{sec};
      return setSecret(tmp);
    }

    //----------------------------------------------------------------------------

    string PasswordProtectedSecret::getSecretAsString()
    {
      SodiumSecureMemory sec = getSecret(SodiumSecureMemType::Normal);

      return (sec.isValid() ? sec.copyToString() : string{});
    }

    //----------------------------------------------------------------------------

    SodiumSecureMemory PasswordProtectedSecret::getSecret(SodiumSecureMemType memType)
    {
      // is a decryption key available?
      if (!(pwClear.isValid()))
      {
        throw NoPasswordSet{};
      }

      // do we have any content at all?
      if (!(cipher.isValid()))
      {
        return SodiumSecureMemory{};
      }

      // decrypt and throw an exception if things fail
      symKey.setAccess(SodiumSecureMemAccess::RO);
      SodiumSecureMemory sec = lib->crypto_secretbox_open_easy__secure(cipher, nonce, symKey, memType);
      symKey.setAccess(SodiumSecureMemAccess::NoAccess);

      if (!(sec.isValid()))
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

      // decrypt the secret and store it in a temporary memory
      SodiumSecureMemory sec = getSecret(SodiumSecureMemType::Normal);
      if ((!(sec.isValid())) && (cipher.isValid())) return false;

      // determine the updated hashing parameters
      tie(hashConfig.opslimit, hashConfig.memlimit) = lib->pwHashConfigToValues(pwStrength, pwAlgo);
      hashConfig.algo = pwAlgo;
      lib->randombytes_buf(hashConfig.salt);

      // re-store the secret
      if (sec.isValid())
      {
        password2SymKey(newPw);
        return setSecret(sec);
      }

      // we have no secret yet, so we just hash the new pw
      password2SymKey(newPw);
      cipher.releaseMemory();

      return true;
    }

    //----------------------------------------------------------------------------

    bool PasswordProtectedSecret::setPassword(const string& pw)
    {
      // this function is only intended for
      // cases if there is no previously set password
      if (pwClear.isValid()) return false;

      if (pw.empty()) return false;

      password2SymKey(pw);

      // if we have content, try to decode it to check
      // the password validity
      if (cipher.isValid())
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
      if (!(pwClear.isValid()))
      {
        throw NoPasswordSet{};
      }

      SodiumSecureMemory pwSec{pw, SodiumSecureMemType::Normal};
      pwClear.setAccess(SodiumSecureMemAccess::RO);
      bool isEqual = lib->memcmp(pwClear, pwSec);
      pwClear.setAccess(SodiumSecureMemAccess::NoAccess);

      return isEqual;
    }

    //----------------------------------------------------------------------------

    string PasswordProtectedSecret::asString(bool useBase64) const
    {
      Net::MessageBuilder mb;

      mb.addByte((hashConfig.algo == SodiumLib::PasswdHashAlgo::Argon2) ? 0 : 1);
      mb.addUI64(hashConfig.memlimit);
      mb.addUI64(hashConfig.opslimit);
      mb.addMemView(hashConfig.salt);  // also works for an empty salt
      mb.addMemView(cipher);  // also works for an empty cipher
      mb.addMemView(nonce);

      // not very efficient... hmmmm...
      string rawData = mb.get().copyToString();

      return useBase64 ? toBase64(rawData) : rawData;
    }

    //----------------------------------------------------------------------------

    void PasswordProtectedSecret::password2SymKey(const string& pw)
    {
      // convert the pw into a hash; the hash will be used as the secret key for
      // encrypting the secret
      SodiumLib::SecretBoxKeyType sk;
      SodiumSecureMemory pwSecure{pw, SodiumSecureMemType::Normal};
      sk = lib->crypto_pwhash(pwSecure, sk.getSize(), hashConfig, sk.getType());
      if (!(sk.isValid()))
      {
        throw PasswordHashingError{};
      }

      symKey.setAccess(SodiumSecureMemAccess::RW);
      symKey = std::move(sk);
      symKey.setAccess(SodiumSecureMemAccess::NoAccess);

      if (pwClear.isValid()) pwClear.setAccess(SodiumSecureMemAccess::RW);
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

  }
}
