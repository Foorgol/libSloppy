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

#include <iostream>

#include <sodium.h>

#include <gtest/gtest.h>
#include "../Sloppy/libSloppy.h"
#include "../Sloppy/Crypto/Sodium.h"

using namespace Sloppy;
using namespace Sloppy::Crypto;

TEST(Sodium, SodiumInit)
{
  SodiumLib* sodium{nullptr};

  ASSERT_NO_THROW(sodium = SodiumLib::getInstance());
  ASSERT_TRUE(sodium != nullptr);
}

//----------------------------------------------------------------------------

TEST(Sodium, Bin2Hex)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  ASSERT_EQ("414243", sodium->bin2hex("ABC"));
  ASSERT_EQ("", sodium->bin2hex(""));

  Sloppy::ManagedBuffer buf{4};
  string tmp{"ABCD"};
  memcpy(buf.get(), tmp.c_str(), 4);
  ASSERT_EQ("41424344", sodium->bin2hex(buf));

}

//----------------------------------------------------------------------------

TEST(Sodium, SecureMem)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  SodiumSecureMemory mem{20, SodiumSecureMemType::Guarded};
  ASSERT_EQ(20, mem.getSize());
  ASSERT_EQ(SodiumSecureMemType::Guarded, mem.getType());

  // the following calls will cause segfaults if
  // the memory management doesn't work
  ASSERT_TRUE(mem.setAccess(SodiumSecureMemAccess::RO));
  char x = ((char*)mem.get())[0];
  ASSERT_TRUE(mem.setAccess(SodiumSecureMemAccess::RW));
  ((char*)mem.get())[0] = 'x';

  // check that normal memory can't be protected
  SodiumSecureMemory mem1{10, SodiumSecureMemType::Normal};
  ASSERT_EQ(10, mem1.getSize());
  ASSERT_EQ(SodiumSecureMemType::Normal, mem1.getType());
  ASSERT_FALSE(mem1.setAccess(SodiumSecureMemAccess::RO));
  ASSERT_FALSE(mem1.setAccess(SodiumSecureMemAccess::RW));
  ASSERT_FALSE(mem1.setAccess(SodiumSecureMemAccess::NoAccess));

  // fill mem1 with values
  char* ptr = (char *)mem1.get();
  for (size_t idx = 0; idx < 10; ++idx)
  {
    ptr[idx] = 'A' + idx;
  }

  // make sure that the move assignment compiles
  mem = std::move(mem1);

  // check that moving was successfull
  ASSERT_EQ(0, mem1.getSize());
  ASSERT_EQ(nullptr, mem1.get());
  ASSERT_EQ(10, mem.getSize());
  ASSERT_TRUE(mem.get() != nullptr);
  ptr = (char *)mem.get();
  for (size_t idx = 0; idx < 10; ++idx)
  {
    ASSERT_EQ('A' + idx, ptr[idx]);
  }

}

//----------------------------------------------------------------------------

TEST(Sodium, SecureMemCopy)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // allocate and fill memory
  SodiumSecureMemory mem{20, SodiumSecureMemType::Guarded};
  char* ptr = (char *)mem.get();
  for (size_t idx = 0; idx < 20; ++idx)
  {
    ptr[idx] = 'A' + idx;
  }

  // protect the memory
  ASSERT_TRUE(mem.setAccess(SodiumSecureMemAccess::NoAccess));

  // try to get a copy
  ASSERT_THROW(SodiumSecureMemory::asCopy(mem), SodiumKeyLocked);

  // unlock and actually create the copy
  ASSERT_TRUE(mem.setAccess(SodiumSecureMemAccess::RO));
  SodiumSecureMemory cpy = SodiumSecureMemory::asCopy(mem);

  // make sure the memory areas are different
  ASSERT_NE(mem.get(), cpy.get());

  // make sure the types are identical
  ASSERT_EQ(mem.getSize(), cpy.getSize());
  ASSERT_EQ(mem.getType(), cpy.getType());

  // make sure protection has been restored
  ASSERT_EQ(mem.getProtection(), cpy.getProtection());
  ASSERT_EQ(SodiumSecureMemAccess::RO, mem.getProtection());

  // check memory contents
  ptr = (char *)cpy.get();
  for (size_t idx = 0; idx < 20; ++idx)
  {
    ASSERT_EQ('A' + idx, ptr[idx]);
  }

  // test conversion from/to string
  mem = SodiumSecureMemory{20000, SodiumSecureMemType::Normal};
  sodium->randombytes_buf(mem);
  string s = mem.copyToString();
  ASSERT_EQ(s.size(), mem.getSize());
  ASSERT_TRUE(s.c_str() != mem.get_c());
  SodiumSecureMemory mem2{s, SodiumSecureMemType::Normal};
  ASSERT_TRUE(sodium->memcmp(mem, mem2));
  ASSERT_TRUE(mem.get() != mem2.get());
}

//----------------------------------------------------------------------------

TEST(Sodium, MemCmp)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // allocate and fill memory
  SodiumSecureMemory mem1{20, SodiumSecureMemType::Guarded};
  char* ptr = (char *)mem1.get();
  for (size_t idx = 0; idx < mem1.getSize(); ++idx)
  {
    ptr[idx] = 'A' + idx;
  }

  ManagedBuffer mem2{mem1.getSize()};
  ptr = (char *)mem2.get();
  for (size_t idx = 0; idx < mem1.getSize(); ++idx)
  {
    ptr[idx] = 'A' + idx;
  }

  ASSERT_TRUE(sodium->memcmp(mem1, mem2));
  ptr[0] = 42;
  ASSERT_FALSE(sodium->memcmp(mem1, mem2));
}

//----------------------------------------------------------------------------

TEST(Sodium, Random)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // allocate and fill memory
  ManagedBuffer buf{20};
  char* ptr = (char *)buf.get();
  for (size_t idx = 0; idx < buf.getSize(); ++idx)
  {
    ptr[idx] = 0;
  }
  ASSERT_TRUE(sodium->isZero(buf));

  // fill with random data
  sodium->randombytes_buf(buf);
  ASSERT_FALSE(sodium->isZero(buf));

  // check the random generator with upper bound
  uint32_t max = 1000;
  for (int cnt = 0; cnt < 10000 ; ++cnt)
  {
    ASSERT_TRUE(sodium->randombytes_uniform(max) < max);
  }
}

//----------------------------------------------------------------------------

TEST(Sodium, SymmetricLowLevel)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random message
  static constexpr size_t msgSize = 500;
  ManagedBuffer msg{msgSize};
  sodium->randombytes_buf(msg);

  // generate random nonce and key
  SodiumLib::SecretBoxNonceType nonce;
  sodium->randombytes_buf(nonce);
  SodiumLib::SecretBoxKeyType key;
  sodium->randombytes_buf(key);

  // encrypt
  ManagedBuffer cipher = sodium->crypto_secretbox_easy(msg, nonce, key);
  ASSERT_TRUE(cipher.isValid());
  ASSERT_EQ(msgSize + crypto_secretbox_MACBYTES, cipher.getSize());

  // decrypt
  SodiumSecureMemory msg2 = sodium->crypto_secretbox_open_easy(cipher, nonce, key);
  ASSERT_TRUE(msg2.isValid());
  ASSERT_EQ(msgSize, msg2.getSize());
  ASSERT_TRUE(sodium->memcmp(msg, msg2));

  // tamper with the cipher text and try again to decrypt
  auto trueCipher = ManagedBuffer::asCopy(cipher);
  char* ptr = cipher.get_c();
  ptr[12] += 1;
  msg2 = sodium->crypto_secretbox_open_easy(cipher, nonce, key);
  ASSERT_FALSE(msg2.isValid());
  ASSERT_EQ(0, msg2.getSize());
  ASSERT_FALSE(sodium->memcmp(msg, msg2));

  // tamper with the key and try again to decrypt
  auto trueKey = SodiumLib::SecretBoxKeyType::asCopy(key);
  ptr = key.get_c();
  ptr[12] += 1;
  msg2 = sodium->crypto_secretbox_open_easy(trueCipher, nonce, key);
  ASSERT_FALSE(msg2.isValid());
  ASSERT_EQ(0, msg2.getSize());
  ASSERT_FALSE(sodium->memcmp(msg, msg2));

  // tamper with the nonce and try again to decrypt
  auto trueNonce = SodiumLib::SecretBoxNonceType::asCopy(nonce);
  ptr = nonce.get_c();
  ptr[12] += 1;
  msg2 = sodium->crypto_secretbox_open_easy(trueCipher, nonce, trueKey);
  ASSERT_FALSE(msg2.isValid());
  ASSERT_EQ(0, msg2.getSize());
  ASSERT_FALSE(sodium->memcmp(msg, msg2));

  //
  // briefly test the detached functions
  //

  ManagedBuffer mac;
  auto tmp = sodium->crypto_secretbox_detached(msg, trueNonce, trueKey);
  cipher = std::move(tmp.first);
  mac = std::move(tmp.second);
  ASSERT_TRUE(cipher.isValid());
  ASSERT_TRUE(mac.isValid());
  ASSERT_EQ(msg.getSize(), cipher.getSize());

  msg2 = sodium->crypto_secretbox_open_detached(cipher, mac, trueNonce, trueKey);
  ASSERT_TRUE(msg2.isValid());
  ASSERT_EQ(msgSize, msg2.getSize());
  ASSERT_TRUE(sodium->memcmp(msg, msg2));
}

//----------------------------------------------------------------------------

TEST(Sodium, SymmetricLowLevel_String)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random message
  static constexpr size_t msgSize = 500;
  ManagedBuffer _msg{msgSize};
  sodium->randombytes_buf(_msg);
  string msg = _msg.copyToString();

  // generate random nonce and key
  ManagedBuffer _nonce{crypto_secretbox_NONCEBYTES};
  sodium->randombytes_buf(_nonce);
  string nonce = _nonce.copyToString();
  SodiumSecureMemory _key{crypto_secretbox_KEYBYTES, SodiumSecureMemType::Normal};
  sodium->randombytes_buf(_key);
  string key = _key.copyToString();

  // encrypt
  string cipher = sodium->crypto_secretbox_easy(msg, nonce, key);
  ASSERT_FALSE(cipher.empty());
  ASSERT_EQ(msgSize + crypto_secretbox_MACBYTES, cipher.size());

  // decrypt
  string msg2 = sodium->crypto_secretbox_open_easy(cipher, nonce, key);
  ASSERT_FALSE(msg2.empty());
  ASSERT_EQ(msgSize, msg2.size());
  ASSERT_EQ(msg, msg2);

  //
  // briefly test the detached functions
  //

  string mac;
  tie(cipher, mac) = sodium->crypto_secretbox_detached(msg, nonce, key);
  ASSERT_FALSE(cipher.empty());
  ASSERT_FALSE(mac.empty());
  ASSERT_EQ(msg.size(), cipher.size());

  msg2 = sodium->crypto_secretbox_open_detached(cipher, mac, nonce, key);
  ASSERT_FALSE(msg2.empty());
  ASSERT_EQ(msgSize, msg2.size());
  ASSERT_EQ(msg, msg2);
}

//----------------------------------------------------------------------------

TEST(Sodium, SecretBoxClass)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random message
  static constexpr size_t msgSize = 500;
  ManagedBuffer _msg{msgSize};
  sodium->randombytes_buf(_msg);
  string msg = _msg.copyToString();

  // generate random nonce and key
  SodiumSecretBox::NonceType nonce;
  sodium->randombytes_buf(nonce);
  SodiumSecretBox::KeyType key;
  sodium->randombytes_buf(key);

  // create the SecretBox
  SodiumSecretBox box{key, nonce};

  // encrypt
  ManagedBuffer cipher = box.encryptCombined(_msg);
  ASSERT_TRUE(cipher.isValid());
  ASSERT_EQ(msgSize + crypto_secretbox_MACBYTES, cipher.getSize());

  // decrypt
  SodiumSecureMemory msg2 = box.decryptCombined(cipher);
  ASSERT_TRUE(msg2.isValid());
  ASSERT_TRUE(sodium->memcmp(_msg, msg2));

  // encrypt detached, as strings
  string c;
  string m;
  tie(c, m) = box.encryptDetached(msg);
  ASSERT_FALSE(c.empty());
  ASSERT_FALSE(m.empty());

  // decrypt detached, as strings
  string s2 = box.decryptDetached(c, m);
  ASSERT_FALSE(s2.empty());
  ASSERT_EQ(msg, s2);
}

//----------------------------------------------------------------------------

TEST(Sodium, SecretBoxClass_NonceInc)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random message
  static constexpr size_t msgSize = 500;
  ManagedBuffer _msg{msgSize};
  sodium->randombytes_buf(_msg);
  string msg = _msg.copyToString();

  // generate random nonce and key
  SodiumSecretBox::NonceType nonce;
  sodium->randombytes_buf(nonce);
  SodiumSecretBox::KeyType key;
  sodium->randombytes_buf(key);

  // create the SecretBox
  SodiumSecretBox box{key, nonce, true};

  // encrypt
  ManagedBuffer cipher = box.encryptCombined(_msg);
  ASSERT_TRUE(cipher.isValid());
  ASSERT_EQ(msgSize + crypto_secretbox_MACBYTES, cipher.getSize());

  // encrypt again
  ManagedBuffer cipher2 = box.encryptCombined(_msg);
  ASSERT_TRUE(cipher2.isValid());

  // both ciphers should differ because the nonce changed
  ASSERT_EQ(cipher.getSize(), cipher2.getSize());
  ASSERT_FALSE(sodium->memcmp(cipher, cipher2));
  ASSERT_EQ(2, box.getNonceIncrementCount());

  // decrypt
  box.setNonce(box.getLastNonce());  // set the right nonce for decryption
  SodiumSecureMemory msg2 = box.decryptCombined(cipher2);
  ASSERT_TRUE(msg2.isValid());
  ASSERT_TRUE(sodium->memcmp(_msg, msg2));

  // encrypt detached, as strings
  string c;
  string m;
  tie(c, m) = box.encryptDetached(msg);
  ASSERT_FALSE(c.empty());
  ASSERT_FALSE(m.empty());

  // decrypt detached, as strings
  box.setNonce(box.getLastNonce());  // set the right nonce for decryption
  string s2 = box.decryptDetached(c, m);
  ASSERT_FALSE(s2.empty());
  ASSERT_EQ(msg, s2);
}

//----------------------------------------------------------------------------

TEST(Sodium, Auth)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random message
  static constexpr size_t msgSize = 500;
  ManagedBuffer msg{msgSize};
  sodium->randombytes_buf(msg);
  string sMsg = msg.copyToString();

  // generate a random key
  SodiumLib::AuthKeyType key;
  sodium->randombytes_buf(key);
  string sKey = key.copyToString();

  // calc an auth tag, buffer-based
  SodiumLib::AuthTagType tag = sodium->crypto_auth(msg, key);
  ASSERT_TRUE(tag.isValid());

  // check the tag, buffer-based
  ASSERT_TRUE(sodium->crypto_auth_verify(msg, tag, key));

  // tamper with the message and see if the check fails
  char* ptr = msg.get_c();
  ptr[5] += 1;
  ASSERT_FALSE(sodium->crypto_auth_verify(msg, tag, key));
  ptr[5] -= 1;
  ASSERT_TRUE(sodium->crypto_auth_verify(msg, tag, key));

  // tamper with the tag and see if the check fails
  ptr = tag.get_c();
  ptr[5] += 1;
  ASSERT_FALSE(sodium->crypto_auth_verify(msg, tag, key));
  ptr[5] -= 1;
  ASSERT_TRUE(sodium->crypto_auth_verify(msg, tag, key));

  // tamper with the key and see if the check fails
  ptr = key.get_c();
  ptr[5] += 1;
  ASSERT_FALSE(sodium->crypto_auth_verify(msg, tag, key));
  ptr[5] -= 1;
  ASSERT_TRUE(sodium->crypto_auth_verify(msg, tag, key));

  // calc an auth tag, string-based
  string sTag = sodium->crypto_auth(sMsg, sKey);
  ASSERT_FALSE(sTag.empty());

  // check the tag, string-based
  ASSERT_TRUE(sodium->crypto_auth_verify(sMsg, sTag, sKey));

  // tamper with the message and see if the check fails
  ptr = (char *)sMsg.c_str();
  ptr[5] += 1;
  ASSERT_FALSE(sodium->crypto_auth_verify(sMsg, sTag, sKey));
  ptr[5] -= 1;
  ASSERT_TRUE(sodium->crypto_auth_verify(sMsg, sTag, sKey));

  // tamper with the tag and see if the check fails
  ptr = (char *)sTag.c_str();
  ptr[5] += 1;
  ASSERT_FALSE(sodium->crypto_auth_verify(sMsg, sTag, sKey));
  ptr[5] -= 1;
  ASSERT_TRUE(sodium->crypto_auth_verify(sMsg, sTag, sKey));

  // tamper with the key and see if the check fails
  ptr = (char *)sKey.c_str();
  ptr[5] += 1;
  ASSERT_FALSE(sodium->crypto_auth_verify(sMsg, sTag, sKey));
  ptr[5] -= 1;
  ASSERT_TRUE(sodium->crypto_auth_verify(sMsg, sTag, sKey));
}

//----------------------------------------------------------------------------

TEST(Sodium, AEAD_ChaCha20)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random message
  static constexpr size_t msgSize = 500;
  ManagedBuffer msg{msgSize};
  sodium->randombytes_buf(msg);
  string sMsg = msg.copyToString();

  // generate a random key
  SodiumLib::AEAD_ChaCha20Poly1305_KeyType key;
  sodium->randombytes_buf(key);
  string sKey = key.copyToString();

  // generate a random nonce
  SodiumLib::AEAD_ChaCha20Poly1305_NonceType nonce;
  sodium->randombytes_buf(nonce);
  string sNonce = nonce.copyToString();

  // generate random extra data
  static constexpr size_t adSize = 500;
  ManagedBuffer ad{adSize};
  sodium->randombytes_buf(ad);
  string sAd = ad.copyToString();

  // encrypt
  auto cipher = sodium->crypto_aead_chacha20poly1305_encrypt(msg, nonce, key, ad);
  ASSERT_TRUE(cipher.isValid());
  ASSERT_TRUE(cipher.getSize() >= (msg.getSize() + crypto_aead_chacha20poly1305_ABYTES));

  // decrypt
  auto msg2 = sodium->crypto_aead_chacha20poly1305_decrypt(cipher, nonce, key, ad, SodiumSecureMemType::Normal);
  ASSERT_TRUE(msg2.isValid());
  ASSERT_EQ(msg.getSize(), msg2.getSize());
  ASSERT_TRUE(sodium->memcmp(msg, msg2));

  // tamper with the additional data, nonce and key
  for (char* ptr : {ad.get_c(), nonce.get_c(), key.get_c()})
  {
    ptr[5] += 1;
    msg2 = sodium->crypto_aead_chacha20poly1305_decrypt(cipher, nonce, key, ad, SodiumSecureMemType::Normal);
    ASSERT_FALSE(msg2.isValid());
    ptr[5] -= 1;
    msg2 = sodium->crypto_aead_chacha20poly1305_decrypt(cipher, nonce, key, ad, SodiumSecureMemType::Normal);
    ASSERT_TRUE(msg2.isValid());
    ASSERT_EQ(msg.getSize(), msg2.getSize());
    ASSERT_TRUE(sodium->memcmp(msg, msg2));
  }

  // decrypt without additional data although AD was provided
  // during encryption
  msg2 = sodium->crypto_aead_chacha20poly1305_decrypt(cipher, nonce, key);
  ASSERT_FALSE(msg2.isValid());

  // encrypt / decrypt without additional data
  cipher = sodium->crypto_aead_chacha20poly1305_encrypt(msg, nonce, key);
  ASSERT_TRUE(cipher.isValid());
  ASSERT_TRUE(cipher.getSize() >= (msg.getSize() + crypto_aead_chacha20poly1305_ABYTES));
  msg2 = sodium->crypto_aead_chacha20poly1305_decrypt(cipher, nonce, key);
  ASSERT_TRUE(msg2.isValid());
  ASSERT_EQ(msg.getSize(), msg2.getSize());
  ASSERT_TRUE(sodium->memcmp(msg, msg2));

  // tamper with nonce and key
  for (char* ptr : {nonce.get_c(), key.get_c()})
  {
    ptr[5] += 1;
    msg2 = sodium->crypto_aead_chacha20poly1305_decrypt(cipher, nonce, key);
    ASSERT_FALSE(msg2.isValid());
    ptr[5] -= 1;
    msg2 = sodium->crypto_aead_chacha20poly1305_decrypt(cipher, nonce, key);
    ASSERT_TRUE(msg2.isValid());
    ASSERT_EQ(msg.getSize(), msg2.getSize());
    ASSERT_TRUE(sodium->memcmp(msg, msg2));
  }

  // decrypt with additional data although AD was provided
  // during encryption
  msg2 = sodium->crypto_aead_chacha20poly1305_decrypt(cipher, nonce, key, ad);
  ASSERT_FALSE(msg2.isValid());

  //
  // test the string-based functions
  //

  // encrypt
  auto sCipher = sodium->crypto_aead_chacha20poly1305_encrypt(sMsg, sNonce, sKey, sAd);
  ASSERT_FALSE(sCipher.empty());
  ASSERT_TRUE(sCipher.size() >= (sMsg.size() + crypto_aead_chacha20poly1305_ABYTES));

  // decrypt
  auto sMsg2 = sodium->crypto_aead_chacha20poly1305_decrypt(sCipher, sNonce, sKey, sAd);
  ASSERT_FALSE(sMsg2.empty());
  ASSERT_EQ(sMsg.size(), sMsg2.size());
  ASSERT_EQ(sMsg, sMsg2);

  // tamper with the additional data, sNonce and sKey
  for (char* ptr : {(char *)sAd.c_str(), (char *)sNonce.c_str(), (char *)sKey.c_str()})
  {
    ptr[5] += 1;
    sMsg2 = sodium->crypto_aead_chacha20poly1305_decrypt(sCipher, sNonce, sKey, sAd);
    ASSERT_TRUE(sMsg2.empty());
    ptr[5] -= 1;
    sMsg2 = sodium->crypto_aead_chacha20poly1305_decrypt(sCipher, sNonce, sKey, sAd);
    ASSERT_FALSE(sMsg2.empty());
    ASSERT_EQ(sMsg.size(), sMsg2.size());
    ASSERT_EQ(sMsg, sMsg2);
  }

  // decrypt without additional data although AD was provided
  // during encryption
  sMsg2 = sodium->crypto_aead_chacha20poly1305_decrypt(sCipher, sNonce, sKey);
  ASSERT_TRUE(sMsg2.empty());

  // encrypt / decrypt without additional data
  sCipher = sodium->crypto_aead_chacha20poly1305_encrypt(sMsg, sNonce, sKey);
  ASSERT_FALSE(sCipher.empty());
  ASSERT_TRUE(sCipher.size() >= (sMsg.size() + crypto_aead_chacha20poly1305_ABYTES));
  sMsg2 = sodium->crypto_aead_chacha20poly1305_decrypt(sCipher, sNonce, sKey);
  ASSERT_FALSE(sMsg2.empty());
  ASSERT_EQ(sMsg.size(), sMsg2.size());
  ASSERT_EQ(sMsg, sMsg2);

  // tamper with sNonce and sKey
  for (char* ptr : {(char *)sNonce.c_str(), (char *)sKey.c_str()})
  {
    ptr[5] += 1;
    sMsg2 = sodium->crypto_aead_chacha20poly1305_decrypt(sCipher, sNonce, sKey);
    ASSERT_TRUE(sMsg2.empty());
    ptr[5] -= 1;
    sMsg2 = sodium->crypto_aead_chacha20poly1305_decrypt(sCipher, sNonce, sKey);
    ASSERT_FALSE(sMsg2.empty());
    ASSERT_EQ(sMsg.size(), sMsg2.size());
    ASSERT_EQ(sMsg, sMsg2);
  }

  // decrypt with additional data although AD was provided
  // during encryption
  sMsg2 = sodium->crypto_aead_chacha20poly1305_decrypt(sCipher, sNonce, sKey, sAd);
  ASSERT_TRUE(sMsg2.empty());
}

//----------------------------------------------------------------------------

TEST(Sodium, AEAD_AES256GCM)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random message
  static constexpr size_t msgSize = 500;
  ManagedBuffer msg{msgSize};
  sodium->randombytes_buf(msg);
  string sMsg = msg.copyToString();

  // generate a random key
  SodiumLib::AEAD_AES256GCM_KeyType key;
  sodium->randombytes_buf(key);
  string sKey = key.copyToString();

  // generate a random nonce
  SodiumLib::AEAD_AES256GCM_NonceType nonce;
  sodium->randombytes_buf(nonce);
  string sNonce = nonce.copyToString();

  // generate random extra data
  static constexpr size_t adSize = 500;
  ManagedBuffer ad{adSize};
  sodium->randombytes_buf(ad);
  string sAd = ad.copyToString();

  // encrypt
  auto cipher = sodium->crypto_aead_aes256gcm_encrypt(msg, nonce, key, ad);
  ASSERT_TRUE(cipher.isValid());
  ASSERT_TRUE(cipher.getSize() >= (msg.getSize() + crypto_aead_aes256gcm_ABYTES));

  // decrypt
  auto msg2 = sodium->crypto_aead_aes256gcm_decrypt(cipher, nonce, key, ad, SodiumSecureMemType::Normal);
  ASSERT_TRUE(msg2.isValid());
  ASSERT_EQ(msg.getSize(), msg2.getSize());
  ASSERT_TRUE(sodium->memcmp(msg, msg2));

  // decrypt without additional data although AD was provided
  // during encryption
  msg2 = sodium->crypto_aead_aes256gcm_decrypt(cipher, nonce, key);
  ASSERT_FALSE(msg2.isValid());

  // encrypt / decrypt without additional data
  cipher = sodium->crypto_aead_aes256gcm_encrypt(msg, nonce, key);
  ASSERT_TRUE(cipher.isValid());
  ASSERT_TRUE(cipher.getSize() >= (msg.getSize() + crypto_aead_aes256gcm_ABYTES));
  msg2 = sodium->crypto_aead_aes256gcm_decrypt(cipher, nonce, key);
  ASSERT_TRUE(msg2.isValid());
  ASSERT_EQ(msg.getSize(), msg2.getSize());
  ASSERT_TRUE(sodium->memcmp(msg, msg2));

  // decrypt with additional data although AD was provided
  // during encryption
  msg2 = sodium->crypto_aead_aes256gcm_decrypt(cipher, nonce, key, ad);
  ASSERT_FALSE(msg2.isValid());

  //
  // test the string-based functions
  //

  // encrypt
  auto sCipher = sodium->crypto_aead_aes256gcm_encrypt(sMsg, sNonce, sKey, sAd);
  ASSERT_FALSE(sCipher.empty());
  ASSERT_TRUE(sCipher.size() >= (sMsg.size() + crypto_aead_aes256gcm_ABYTES));

  // decrypt
  auto sMsg2 = sodium->crypto_aead_aes256gcm_decrypt(sCipher, sNonce, sKey, sAd);
  ASSERT_FALSE(sMsg2.empty());
  ASSERT_EQ(sMsg.size(), sMsg2.size());
  ASSERT_EQ(sMsg, sMsg2);

  // decrypt without additional data although AD was provided
  // during encryption
  sMsg2 = sodium->crypto_aead_aes256gcm_decrypt(sCipher, sNonce, sKey);
  ASSERT_TRUE(sMsg2.empty());

  // encrypt / decrypt without additional data
  sCipher = sodium->crypto_aead_aes256gcm_encrypt(sMsg, sNonce, sKey);
  ASSERT_FALSE(sCipher.empty());
  ASSERT_TRUE(sCipher.size() >= (sMsg.size() + crypto_aead_aes256gcm_ABYTES));
  sMsg2 = sodium->crypto_aead_aes256gcm_decrypt(sCipher, sNonce, sKey);
  ASSERT_FALSE(sMsg2.empty());
  ASSERT_EQ(sMsg.size(), sMsg2.size());
  ASSERT_EQ(sMsg, sMsg2);

  // decrypt with additional data although AD was provided
  // during encryption
  sMsg2 = sodium->crypto_aead_aes256gcm_decrypt(sCipher, sNonce, sKey, sAd);
  ASSERT_TRUE(sMsg2.empty());
}

//----------------------------------------------------------------------------

TEST(Sodium, AsymKeyHandling)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random key pair
  SodiumLib::AsymCrypto_PublicKey pk;
  SodiumLib::AsymCrypto_SecretKey sk;
  sodium->genAsymKeyPair(pk, sk);

  // re-gen the public key from the secret key
  SodiumLib::AsymCrypto_PublicKey pk2;
  ASSERT_TRUE(sodium->genPublicKeyFromSecretKey(sk, pk2));
  ASSERT_TRUE(sodium->memcmp(pk, pk2));

  // gen key pair from seed
  SodiumLib::AsymCrypto_KeySeed seed;
  sodium->randombytes_buf(seed);
  sodium->genAsymKeyPairSeeded(seed, pk, sk);
  SodiumLib::AsymCrypto_SecretKey sk2;
  sodium->genAsymKeyPairSeeded(seed, pk2, sk2);
  ASSERT_TRUE(sodium->memcmp(pk, pk2));
  ASSERT_TRUE(sodium->memcmp(sk, sk2));
}

//----------------------------------------------------------------------------

TEST(Sodium, AsymKeyCrypto_Buffer)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random key pair for sender and recipient
  SodiumLib::AsymCrypto_PublicKey pkSender;
  SodiumLib::AsymCrypto_SecretKey skSender;
  sodium->genAsymKeyPair(pkSender, skSender);
  SodiumLib::AsymCrypto_PublicKey pkRecipient;
  SodiumLib::AsymCrypto_SecretKey skRecipient;
  sodium->genAsymKeyPair(pkRecipient, skRecipient);

  // generate a random message
  static constexpr size_t msgSize = 500;
  ManagedBuffer msg{msgSize};
  sodium->randombytes_buf(msg);

  // generate a nonce
  SodiumLib::AsymCrypto_Nonce nonce;
  sodium->randombytes_buf(nonce);

  // encrypt a message
  auto cipher = sodium->crypto_box_easy(msg, nonce, pkRecipient, skSender);
  ASSERT_TRUE(cipher.isValid());
  ASSERT_EQ(msgSize + crypto_box_MACBYTES, cipher.getSize());

  // decrypt the message
  auto msg2 = sodium->crypto_box_open_easy(cipher, nonce, pkSender, skRecipient);
  ASSERT_TRUE(msg2.isValid());
  ASSERT_EQ(msgSize, msg2.getSize());
  ASSERT_TRUE(sodium->memcmp(msg, msg2));

  // tamper with PK, SK, nonce and ciper
  for (char* ptr : {pkSender.get_c(), skRecipient.get_c(), nonce.get_c(), cipher.get_c()})
  {
    ptr[5] += 1;
    msg2 = sodium->crypto_box_open_easy(cipher, nonce, pkSender, skRecipient);
    ASSERT_FALSE(msg2.isValid());
    ptr[5] -= 1;
    msg2 = sodium->crypto_box_open_easy(cipher, nonce, pkSender, skRecipient);
    ASSERT_TRUE(msg2.isValid());
    ASSERT_EQ(msgSize, msg2.getSize());
    ASSERT_TRUE(sodium->memcmp(msg, msg2));
  }

  //
  // test the detached versions
  //

  SodiumLib::AsymCrypto_Tag mac;
  auto tmp = sodium->crypto_box_detached(msg, nonce, pkRecipient, skSender);
  cipher = std::move(tmp.first);
  mac = std::move(tmp.second);
  ASSERT_TRUE(cipher.isValid());
  ASSERT_EQ(msgSize, cipher.getSize());
  ASSERT_TRUE(mac.isValid());
  ASSERT_EQ(crypto_box_MACBYTES, mac.getSize());

  msg2 = sodium->crypto_box_open_detached(cipher, mac, nonce, pkSender, skRecipient);
  ASSERT_TRUE(msg2.isValid());
  ASSERT_EQ(msgSize, msg2.getSize());
  ASSERT_TRUE(sodium->memcmp(msg, msg2));

  // tamper with PK, SK, nonce, mac and ciper
  for (char* ptr : {pkSender.get_c(), skRecipient.get_c(), nonce.get_c(), mac.get_c(), cipher.get_c()})
  {
    ptr[5] += 1;
    msg2 = sodium->crypto_box_open_detached(cipher, mac, nonce, pkSender, skRecipient);
    ASSERT_FALSE(msg2.isValid());
    ptr[5] -= 1;
    msg2 = sodium->crypto_box_open_detached(cipher, mac, nonce, pkSender, skRecipient);
    ASSERT_TRUE(msg2.isValid());
    ASSERT_EQ(msgSize, msg2.getSize());
    ASSERT_TRUE(sodium->memcmp(msg, msg2));
  }

}

//----------------------------------------------------------------------------

TEST(Sodium, AsymKeyCrypto_String)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random key pair for sender and recipient
  SodiumLib::AsymCrypto_PublicKey pkSender;
  SodiumLib::AsymCrypto_SecretKey skSender;
  sodium->genAsymKeyPair(pkSender, skSender);
  SodiumLib::AsymCrypto_PublicKey pkRecipient;
  SodiumLib::AsymCrypto_SecretKey skRecipient;
  sodium->genAsymKeyPair(pkRecipient, skRecipient);

  // generate a random message
  static constexpr size_t msgSize = 500;
  ManagedBuffer _msg{msgSize};
  sodium->randombytes_buf(_msg);
  string msg = _msg.copyToString();

  // generate a nonce
  SodiumLib::AsymCrypto_Nonce nonce;
  sodium->randombytes_buf(nonce);

  // encrypt a message
  string cipher = sodium->crypto_box_easy(msg, nonce, pkRecipient, skSender);
  ASSERT_FALSE(cipher.empty());
  ASSERT_EQ(msgSize + crypto_box_MACBYTES, cipher.size());

  // decrypt the message
  string msg2 = sodium->crypto_box_open_easy(cipher, nonce, pkSender, skRecipient);
  ASSERT_FALSE(msg2.empty());
  ASSERT_EQ(msgSize, msg2.size());
  ASSERT_EQ(msg, msg2);

  // tamper with PK, SK, nonce and ciper
  for (char* ptr : {pkSender.get_c(), skRecipient.get_c(), nonce.get_c(), (char *)cipher.c_str()})
  {
    ptr[5] += 1;
    msg2 = sodium->crypto_box_open_easy(cipher, nonce, pkSender, skRecipient);
    ASSERT_TRUE(msg2.empty());
    ptr[5] -= 1;
    msg2 = sodium->crypto_box_open_easy(cipher, nonce, pkSender, skRecipient);
    ASSERT_FALSE(msg2.empty());
    ASSERT_EQ(msgSize, msg2.size());
    ASSERT_EQ(msg, msg2);
  }

  //
  // test the detached versions
  //

  string mac;
  tie(cipher, mac) = sodium->crypto_box_detached(msg, nonce, pkRecipient, skSender);
  ASSERT_FALSE(cipher.empty());
  ASSERT_EQ(msgSize, cipher.size());
  ASSERT_FALSE(mac.empty());
  ASSERT_EQ(crypto_box_MACBYTES, mac.size());

  msg2 = sodium->crypto_box_open_detached(cipher, mac, nonce, pkSender, skRecipient);
  ASSERT_FALSE(msg2.empty());
  ASSERT_EQ(msgSize, msg2.size());
  ASSERT_EQ(msg, msg2);

  // tamper with PK, SK, nonce, mac and ciper
  for (char* ptr : {pkSender.get_c(), skRecipient.get_c(), nonce.get_c(), (char *)mac.c_str(), (char *)cipher.c_str()})
  {
    ptr[5] += 1;
    msg2 = sodium->crypto_box_open_detached(cipher, mac, nonce, pkSender, skRecipient);
    ASSERT_TRUE(msg2.empty());
    ptr[5] -= 1;
    msg2 = sodium->crypto_box_open_detached(cipher, mac, nonce, pkSender, skRecipient);
    ASSERT_FALSE(msg2.empty());
    ASSERT_EQ(msgSize, msg2.size());
    ASSERT_EQ(msg, msg2);
  }

}
