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

#include <iostream>

#include <sodium.h>

#include <gtest/gtest.h>
#include "../Sloppy/Crypto/Sodium.h"
#include "../Sloppy/Crypto/Crypto.h"

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

  MemArray buf{4};
  string tmp{"ABCD"};
  memcpy(buf.to_voidPtr(), tmp.c_str(), 4);

  // string-based calls
  string hex = sodium->bin2hex(tmp);
  ASSERT_EQ("41424344", hex);

  // MemView based calls
  MemArray ma = sodium->bin2hex(buf.view());
  ASSERT_EQ(hex.size() + 1, ma.size());  // the buffer contains a terminating zero!
  ma.resize(hex.size());
  ASSERT_TRUE(sodium->memcmp(ma.view(), MemView{hex}));

}

//----------------------------------------------------------------------------

TEST(Sodium, SecureMem)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  SodiumSecureMemory mem{20, SodiumSecureMemType::Guarded};
  ASSERT_EQ(20, mem.size());
  ASSERT_EQ(SodiumSecureMemType::Guarded, mem.getType());

  // the following calls will cause segfaults if
  // the memory management doesn't work
  ASSERT_TRUE(mem.setAccess(SodiumSecureMemAccess::RO));
  char x = mem.toMemView()[0];
  ASSERT_TRUE(mem.setAccess(SodiumSecureMemAccess::RW));
  mem.toNotOwningArray()[0] = 'x';

  // check that normal memory can't be protected
  SodiumSecureMemory mem1{10, SodiumSecureMemType::Normal};
  ASSERT_EQ(10, mem1.size());
  ASSERT_EQ(SodiumSecureMemType::Normal, mem1.getType());
  ASSERT_FALSE(mem1.setAccess(SodiumSecureMemAccess::RO));
  ASSERT_FALSE(mem1.setAccess(SodiumSecureMemAccess::RW));
  ASSERT_FALSE(mem1.setAccess(SodiumSecureMemAccess::NoAccess));

  // fill mem1 with values
  char* ptr = reinterpret_cast<char *>(mem1.to_ucPtr_rw());
  for (size_t idx = 0; idx < 10; ++idx)
  {
    ptr[idx] = 'A' + idx;
  }

  // make sure that the move assignment compiles
  mem = std::move(mem1);

  // check that moving was successfull
  ASSERT_EQ(0, mem1.size());
  ASSERT_EQ(nullptr, mem1.to_ucPtr_ro());
  ASSERT_EQ(10, mem.size());
  ASSERT_TRUE(mem.to_ucPtr_ro() != nullptr);
  ptr = reinterpret_cast<char *>(mem.to_ucPtr_rw());
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
  unsigned char* ptr = mem.to_ucPtr_rw();
  for (size_t idx = 0; idx < 20; ++idx)
  {
    ptr[idx] = 'A' + idx;
  }

  // protect the memory
  ASSERT_TRUE(mem.setAccess(SodiumSecureMemAccess::NoAccess));

  // try to get a copy
  ASSERT_THROW(SodiumSecureMemory{mem}, SodiumMemoryGuardException);

  // unlock and actually create the copy
  ASSERT_TRUE(mem.setAccess(SodiumSecureMemAccess::RO));
  SodiumSecureMemory cpy{mem};

  // make sure the memory areas are different
  ASSERT_NE(mem.to_ucPtr_ro(), cpy.to_ucPtr_ro());

  // make sure the types are identical
  ASSERT_EQ(mem.size(), cpy.size());
  ASSERT_EQ(mem.getType(), cpy.getType());

  // make sure protection has been restored
  ASSERT_EQ(mem.getProtection(), cpy.getProtection());
  ASSERT_EQ(SodiumSecureMemAccess::RO, mem.getProtection());

  // check memory contents
  ptr = cpy.to_ucPtr_rw();
  for (size_t idx = 0; idx < 20; ++idx)
  {
    ASSERT_EQ('A' + idx, ptr[idx]);
  }

}

//----------------------------------------------------------------------------

TEST(Sodium, MemCmp)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // allocate and fill memory
  SodiumSecureMemory mem1{20, SodiumSecureMemType::Guarded};
  char* ptr = reinterpret_cast<char *>(mem1.to_ucPtr_rw());
  for (size_t idx = 0; idx < mem1.size(); ++idx)
  {
    ptr[idx] = 'A' + idx;
  }

  MemArray mem2{mem1.size()};
  ptr = mem2.to_charPtr();
  for (size_t idx = 0; idx < mem1.size(); ++idx)
  {
    ptr[idx] = 'A' + idx;
  }

  ASSERT_TRUE(sodium->memcmp(mem1.toMemView(), mem2.view()));
  ptr[0] = 42;
  ASSERT_FALSE(sodium->memcmp(mem1.toMemView(), mem2.view()));
}

//----------------------------------------------------------------------------

TEST(Sodium, Random)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // allocate and fill memory
  MemArray buf{20};
  char* ptr = buf.to_charPtr();
  for (size_t idx = 0; idx < buf.size(); ++idx)
  {
    ptr[idx] = 0;
  }
  ASSERT_TRUE(sodium->isZero(buf.view()));

  // fill with random data
  sodium->randombytes_buf(buf);
  ASSERT_FALSE(sodium->isZero(buf.view()));

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
  MemArray msg{msgSize};
  sodium->randombytes_buf(msg);

  // generate random nonce and key
  SodiumLib::SecretBoxNonce nonce{SodiumKeyInitStyle::Random};
  SodiumLib::SecretBoxKey key{SodiumKeyInitStyle::Random};

  // encrypt
  MemArray cipher = sodium->secretbox_easy(msg.view(), nonce, key);
  ASSERT_TRUE(cipher.notEmpty());
  ASSERT_EQ(msgSize + crypto_secretbox_MACBYTES, cipher.size());

  // decrypt
  SodiumSecureMemory msg2 = sodium->secretbox_open_easy__secure(cipher.view(), nonce, key);
  ASSERT_FALSE(msg2.empty());
  ASSERT_EQ(msgSize, msg2.size());
  ASSERT_TRUE(sodium->memcmp(msg.view(), msg2.toMemView()));

  // tamper with the cipher text and try again to decrypt
  auto trueCipher{cipher};
  char* ptr = cipher.to_charPtr();
  ptr[12] += 1;
  msg2 = sodium->secretbox_open_easy__secure(cipher.view(), nonce, key);
  ASSERT_TRUE(msg2.empty());
  ASSERT_EQ(0, msg2.size());
  ASSERT_FALSE(sodium->memcmp(msg.view(), msg2.toMemView()));

  // tamper with the key and try again to decrypt
  SodiumLib::SecretBoxKey trueKey{key};
  ptr = reinterpret_cast<char *>(key.to_ucPtr_rw());
  ptr[12] += 1;
  msg2 = sodium->secretbox_open_easy__secure(trueCipher.view(), nonce, key);
  ASSERT_TRUE(msg2.empty());
  ASSERT_EQ(0, msg2.size());
  ASSERT_FALSE(sodium->memcmp(msg.view(), msg2.toMemView()));

  // tamper with the nonce and try again to decrypt
  SodiumLib::SecretBoxNonce trueNonce{nonce};
  ptr = reinterpret_cast<char *>(nonce.to_ucPtr_rw());
  ptr[12] += 1;
  msg2 = sodium->secretbox_open_easy__secure(trueCipher.view(), nonce, trueKey);
  ASSERT_TRUE(msg2.empty());
  ASSERT_EQ(0, msg2.size());
  ASSERT_FALSE(sodium->memcmp(msg.view(), msg2.toMemView()));

  //
  // briefly test the detached functions
  //

  SodiumLib::SecretBoxMac mac;
  auto tmp = sodium->secretbox_detached(msg.view(), trueNonce, trueKey);
  cipher = std::move(tmp.first);
  mac = std::move(tmp.second);
  ASSERT_TRUE(cipher.notEmpty());
  ASSERT_FALSE(mac.empty());
  ASSERT_EQ(msg.size(), cipher.size());

  msg2 = sodium->secretbox_open_detached(cipher.view(), mac, trueNonce, trueKey);
  ASSERT_FALSE(msg2.empty());
  ASSERT_EQ(msgSize, msg2.size());
  ASSERT_TRUE(sodium->memcmp(msg.view(), msg2.toMemView()));
}

//----------------------------------------------------------------------------

TEST(Sodium, SymmetricLowLevel_String)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random message
  static constexpr size_t msgSize = 500;
  MemArray _msg{msgSize};
  sodium->randombytes_buf(_msg);
  string msg{_msg.to_charPtr(), msgSize};

  // generate random nonce and key
  SodiumLib::SecretBoxNonce nonce{SodiumKeyInitStyle::Random};
  SodiumLib::SecretBoxKey key{SodiumKeyInitStyle::Random};

  // encrypt
  string cipher = sodium->secretbox_easy(msg, nonce, key);
  ASSERT_FALSE(cipher.empty());
  ASSERT_EQ(msgSize + crypto_secretbox_MACBYTES, cipher.size());

  // decrypt
  string msg2 = sodium->secretbox_open_easy(cipher, nonce, key);
  ASSERT_FALSE(msg2.empty());
  ASSERT_EQ(msgSize, msg2.size());
  ASSERT_EQ(msg, msg2);

  //
  // briefly test the detached functions
  //

  SodiumLib::SecretBoxMac mac;
  tie(cipher, mac) = sodium->secretbox_detached(msg, nonce, key);
  ASSERT_FALSE(cipher.empty());
  ASSERT_FALSE(mac.empty());
  ASSERT_EQ(msg.size(), cipher.size());

  msg2 = sodium->secretbox_open_detached(cipher, mac, nonce, key);
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
  MemArray _msg{msgSize};
  sodium->randombytes_buf(_msg);
  string msg{_msg.to_charPtr(), msgSize};

  // generate random nonce and key
  SodiumLib::SecretBoxNonce nonce{SodiumKeyInitStyle::Random};
  SodiumLib::SecretBoxKey key{SodiumKeyInitStyle::Random};

  // create the SecretBox
  SodiumSecretBox box{key, nonce};

  // encrypt
  MemArray cipher = box.encryptCombined(_msg.view());
  ASSERT_TRUE(cipher.notEmpty());
  ASSERT_EQ(msgSize + crypto_secretbox_MACBYTES, cipher.size());

  // decrypt
  SodiumSecureMemory msg2 = box.decryptCombined(cipher.view());
  ASSERT_FALSE(msg2.empty());
  ASSERT_TRUE(sodium->memcmp(_msg.view(), msg2.toMemView()));

  // encrypt detached, as strings
  string c;
  SodiumLib::SecretBoxMac m;
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
  MemArray _msg{msgSize};
  sodium->randombytes_buf(_msg);
  string msg{_msg.to_charPtr(), msgSize};

  // generate random nonce and key
  SodiumLib::SecretBoxNonce nonce{SodiumKeyInitStyle::Random};
  SodiumLib::SecretBoxKey key{SodiumKeyInitStyle::Random};

  // create a SecretBox for encryption
  SodiumSecretBox encBox{key, nonce, true};

  // encrypt
  MemArray cipher = encBox.encryptCombined(_msg.view());
  ASSERT_TRUE(cipher.notEmpty());
  ASSERT_EQ(msgSize + crypto_secretbox_MACBYTES, cipher.size());

  // encrypt again
  MemArray cipher2 = encBox.encryptCombined(_msg.view());
  ASSERT_TRUE(cipher2.notEmpty());

  // both ciphers should differ because the nonce changed
  ASSERT_EQ(cipher.size(), cipher2.size());
  ASSERT_FALSE(sodium->memcmp(cipher.view(), cipher2.view()));

  // decrypt in a separate box
  SodiumSecretBox deBox{key, nonce, true};
  SodiumSecureMemory d1 = deBox.decryptCombined(cipher.view());
  ASSERT_FALSE(d1.empty());
  SodiumSecureMemory d2 = deBox.decryptCombined(cipher2.view());
  ASSERT_FALSE(d2.empty());
  ASSERT_TRUE(sodium->memcmp(d1.toMemView(), d2.toMemView()));

  // encrypt detached, as strings
  string c;
  SodiumLib::SecretBoxMac m;
  tie(c, m) = encBox.encryptDetached(msg);
  ASSERT_FALSE(c.empty());
  ASSERT_FALSE(m.empty());
  ASSERT_EQ(c.size(), msg.size());

  // decrypt detached, as strings
  string s2 = deBox.decryptDetached(c, m);
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
  MemArray msg{msgSize};
  sodium->randombytes_buf(msg);
  string sMsg{msg.to_charPtr(), msgSize};

  // generate a random key
  SodiumLib::AuthKeyType key{SodiumKeyInitStyle::Random};

  // calc an auth tag, buffer-based
  SodiumLib::AuthTagType tag = sodium->auth(msg.view(), key);
  ASSERT_FALSE(tag.empty());

  // check the tag, buffer-based
  ASSERT_TRUE(sodium->auth_verify(msg.view(), tag, key));

  // tamper with the message and see if the check fails
  char* ptr = msg.to_charPtr();
  ptr[5] += 1;
  ASSERT_FALSE(sodium->auth_verify(msg.view(), tag, key));
  ptr[5] -= 1;
  ASSERT_TRUE(sodium->auth_verify(msg.view(), tag, key));

  // tamper with the tag and see if the check fails
  ptr = reinterpret_cast<char *>(tag.to_ucPtr_rw());
  ptr[5] += 1;
  ASSERT_FALSE(sodium->auth_verify(msg.view(), tag, key));
  ptr[5] -= 1;
  ASSERT_TRUE(sodium->auth_verify(msg.view(), tag, key));

  // tamper with the key and see if the check fails
  ptr = reinterpret_cast<char *>(key.to_ucPtr_rw());
  ptr[5] += 1;
  ASSERT_FALSE(sodium->auth_verify(msg.view(), tag, key));
  ptr[5] -= 1;
  ASSERT_TRUE(sodium->auth_verify(msg.view(), tag, key));

  // calc an auth tag, string-based
  tag = sodium->auth(sMsg, key);
  ASSERT_FALSE(tag.empty());

  // check the tag, string-based
  ASSERT_TRUE(sodium->auth_verify(sMsg, tag, key));

  // tamper with the message and see if the check fails
  ptr = (char *)sMsg.c_str();
  ptr[5] += 1;
  ASSERT_FALSE(sodium->auth_verify(sMsg, tag, key));
  ptr[5] -= 1;
  ASSERT_TRUE(sodium->auth_verify(sMsg, tag, key));

  // tamper with the tag and see if the check fails
  ptr = reinterpret_cast<char *>(tag.to_ucPtr_rw());
  ptr[5] += 1;
  ASSERT_FALSE(sodium->auth_verify(sMsg, tag, key));
  ptr[5] -= 1;
  ASSERT_TRUE(sodium->auth_verify(sMsg, tag, key));

  // tamper with the key and see if the check fails
  ptr = reinterpret_cast<char *>(key.to_ucPtr_rw());
  ptr[5] += 1;
  ASSERT_FALSE(sodium->auth_verify(sMsg, tag, key));
  ptr[5] -= 1;
  ASSERT_TRUE(sodium->auth_verify(sMsg, tag, key));
}

//----------------------------------------------------------------------------

TEST(Sodium, AEAD_ChaCha20)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random message
  static constexpr size_t msgSize = 500;
  MemArray msg{msgSize};
  sodium->randombytes_buf(msg);
  string sMsg{msg.to_charPtr(), msgSize};

  // generate a random key and nonce
  SodiumLib::AEAD_XChaCha20Poly1305_KeyType key{SodiumKeyInitStyle::Random};
  SodiumLib::AEAD_XChaCha20Poly1305_NonceType nonce{SodiumKeyInitStyle::Random};

  // generate random extra data
  static constexpr size_t adSize = 500;
  MemArray ad{adSize};
  sodium->randombytes_buf(ad);
  string sAd{ad.to_charPtr(), adSize};

  // encrypt
  auto cipher = sodium->aead_xchacha20poly1305_encrypt(msg.view(), nonce, key, ad.view());
  ASSERT_FALSE(cipher.empty());
  ASSERT_TRUE(cipher.size() >= (msg.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES));

  // decrypt
  auto msg2 = sodium->aead_xchacha20poly1305_decrypt(cipher.view(), nonce, key, ad.view(), SodiumSecureMemType::Normal);
  ASSERT_FALSE(msg2.empty());
  ASSERT_EQ(msg.size(), msg2.size());
  ASSERT_TRUE(sodium->memcmp(msg.view(), msg2.toMemView()));

  // tamper with the additional data, nonce and key
  for (char* ptr : {ad.to_charPtr(), (char *)nonce.to_ucPtr_rw(), (char *)key.to_ucPtr_rw()})
  {
    ptr[5] += 1;
    msg2 = sodium->aead_xchacha20poly1305_decrypt(cipher.view(), nonce, key, ad.view(), SodiumSecureMemType::Normal);
    ASSERT_TRUE(msg2.empty());
    ptr[5] -= 1;
    msg2 = sodium->aead_xchacha20poly1305_decrypt(cipher.view(), nonce, key, ad.view(), SodiumSecureMemType::Normal);
    ASSERT_FALSE(msg2.empty());
    ASSERT_EQ(msg.size(), msg2.size());
    ASSERT_TRUE(sodium->memcmp(msg.view(), msg2.toMemView()));
  }

  // decrypt without additional data although AD was provided
  // during encryption
  msg2 = sodium->aead_xchacha20poly1305_decrypt(cipher.view(), nonce, key);
  ASSERT_TRUE(msg2.empty());

  // encrypt / decrypt without additional data
  cipher = sodium->aead_xchacha20poly1305_encrypt(msg.view(), nonce, key);
  ASSERT_FALSE(cipher.empty());
  ASSERT_TRUE(cipher.size() >= (msg.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES));
  msg2 = sodium->aead_xchacha20poly1305_decrypt(cipher.view(), nonce, key);
  ASSERT_FALSE(msg2.empty());
  ASSERT_EQ(msg.size(), msg2.size());
  ASSERT_TRUE(sodium->memcmp(msg.view(), msg2.toMemView()));

  // tamper with nonce and key
  for (char* ptr : {(char *)nonce.to_ucPtr_rw(), (char *)key.to_ucPtr_rw()})
  {
    ptr[5] += 1;
    msg2 = sodium->aead_xchacha20poly1305_decrypt(cipher.view(), nonce, key);
    ASSERT_TRUE(msg2.empty());
    ptr[5] -= 1;
    msg2 = sodium->aead_xchacha20poly1305_decrypt(cipher.view(), nonce, key);
    ASSERT_FALSE(msg2.empty());
    ASSERT_EQ(msg.size(), msg2.size());
    ASSERT_TRUE(sodium->memcmp(msg.view(), msg2.toMemView()));
  }

  // decrypt with additional data although no AD was provided
  // during encryption
  msg2 = sodium->aead_xchacha20poly1305_decrypt(cipher.view(), nonce, key, ad.view());
  ASSERT_TRUE(msg2.empty());

  //
  // test the string-based functions
  //

  // encrypt
  auto sCipher = sodium->aead_xchacha20poly1305_encrypt(sMsg, nonce, key, sAd);
  ASSERT_FALSE(sCipher.empty());
  ASSERT_TRUE(sCipher.size() >= (sMsg.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES));

  // decrypt
  auto sMsg2 = sodium->aead_xchacha20poly1305_decrypt(sCipher, nonce, key, sAd);
  ASSERT_FALSE(sMsg2.empty());
  ASSERT_EQ(sMsg.size(), sMsg2.size());
  ASSERT_EQ(sMsg, sMsg2);

  // tamper with the additional data, sNonce and sKey
  for (char* ptr : {(char *)sAd.c_str(), (char *)nonce.to_ucPtr_rw(), (char *)key.to_ucPtr_rw()})
  {
    ptr[5] += 1;
    sMsg2 = sodium->aead_xchacha20poly1305_decrypt(sCipher, nonce, key, sAd);
    ASSERT_TRUE(sMsg2.empty());
    ptr[5] -= 1;
    sMsg2 = sodium->aead_xchacha20poly1305_decrypt(sCipher, nonce, key, sAd);
    ASSERT_FALSE(sMsg2.empty());
    ASSERT_EQ(sMsg.size(), sMsg2.size());
    ASSERT_EQ(sMsg, sMsg2);
  }

  // decrypt without additional data although AD was provided
  // during encryption
  sMsg2 = sodium->aead_xchacha20poly1305_decrypt(sCipher, nonce, key);
  ASSERT_TRUE(sMsg2.empty());

  // encrypt / decrypt without additional data
  sCipher = sodium->aead_xchacha20poly1305_encrypt(sMsg, nonce, key);
  ASSERT_FALSE(sCipher.empty());
  ASSERT_TRUE(sCipher.size() >= (sMsg.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES));
  sMsg2 = sodium->aead_xchacha20poly1305_decrypt(sCipher, nonce, key);
  ASSERT_FALSE(sMsg2.empty());
  ASSERT_EQ(sMsg.size(), sMsg2.size());
  ASSERT_EQ(sMsg, sMsg2);

  // tamper with sNonce and sKey
  for (char* ptr : {(char *)nonce.to_ucPtr_rw(), (char *)key.to_ucPtr_rw()})
  {
    ptr[5] += 1;
    sMsg2 = sodium->aead_xchacha20poly1305_decrypt(sCipher, nonce, key);
    ASSERT_TRUE(sMsg2.empty());
    ptr[5] -= 1;
    sMsg2 = sodium->aead_xchacha20poly1305_decrypt(sCipher, nonce, key);
    ASSERT_FALSE(sMsg2.empty());
    ASSERT_EQ(sMsg.size(), sMsg2.size());
    ASSERT_EQ(sMsg, sMsg2);
  }

  // decrypt with additional data although AD was provided
  // during encryption
  sMsg2 = sodium->aead_xchacha20poly1305_decrypt(sCipher, nonce, key, sAd);
  ASSERT_TRUE(sMsg2.empty());
}

//----------------------------------------------------------------------------

TEST(Sodium, AEAD_AES256GCM)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random message
  static constexpr size_t msgSize = 500;
  MemArray msg{msgSize};
  sodium->randombytes_buf(msg);
  string sMsg{msg.to_charPtr(), msgSize};

  // generate a random key and nonce
  SodiumLib::AEAD_AES256GCM_KeyType key{SodiumKeyInitStyle::Random};
  SodiumLib::AEAD_AES256GCM_NonceType nonce{SodiumKeyInitStyle::Random};

  // generate random extra data
  static constexpr size_t adSize = 500;
  MemArray ad{adSize};
  sodium->randombytes_buf(ad);
  string sAd{ad.to_charPtr(), adSize};

  // encrypt
  auto cipher = sodium->aead_aes256gcm_encrypt(msg.view(), nonce, key, ad.view());
  ASSERT_FALSE(cipher.empty());
  ASSERT_TRUE(cipher.size() >= (msg.size() + crypto_aead_aes256gcm_ABYTES));

  // decrypt
  auto msg2 = sodium->aead_aes256gcm_decrypt(cipher.view(), nonce, key, ad.view(), SodiumSecureMemType::Normal);
  ASSERT_FALSE(msg2.empty());
  ASSERT_EQ(msg.size(), msg2.size());
  ASSERT_TRUE(sodium->memcmp(msg.view(), msg2.toMemView()));

  // decrypt without additional data although AD was provided
  // during encryption
  msg2 = sodium->aead_aes256gcm_decrypt(cipher.view(), nonce, key);
  ASSERT_TRUE(msg2.empty());

  // encrypt / decrypt without additional data
  cipher = sodium->aead_aes256gcm_encrypt(msg.view(), nonce, key);
  ASSERT_FALSE(cipher.empty());
  ASSERT_TRUE(cipher.size() >= (msg.size() + crypto_aead_aes256gcm_ABYTES));
  msg2 = sodium->aead_aes256gcm_decrypt(cipher.view(), nonce, key);
  ASSERT_FALSE(msg2.empty());
  ASSERT_EQ(msg.size(), msg2.size());
  ASSERT_TRUE(sodium->memcmp(msg.view(), msg2.toMemView()));

  // decrypt with additional data although AD was not provided
  // during encryption
  msg2 = sodium->aead_aes256gcm_decrypt(cipher.view(), nonce, key, ad.view());
  ASSERT_TRUE(msg2.empty());

  //
  // test the string-based functions
  //

  // encrypt
  auto sCipher = sodium->aead_aes256gcm_encrypt(sMsg, nonce, key, sAd);
  ASSERT_FALSE(sCipher.empty());
  ASSERT_TRUE(sCipher.size() >= (sMsg.size() + crypto_aead_aes256gcm_ABYTES));

  // decrypt
  auto sMsg2 = sodium->aead_aes256gcm_decrypt(sCipher, nonce, key, sAd);
  ASSERT_FALSE(sMsg2.empty());
  ASSERT_EQ(sMsg.size(), sMsg2.size());
  ASSERT_EQ(sMsg, sMsg2);

  // decrypt without additional data although AD was provided
  // during encryption
  sMsg2 = sodium->aead_aes256gcm_decrypt(sCipher, nonce, key);
  ASSERT_TRUE(sMsg2.empty());

  // encrypt / decrypt without additional data
  sCipher = sodium->aead_aes256gcm_encrypt(sMsg, nonce, key);
  ASSERT_FALSE(sCipher.empty());
  ASSERT_TRUE(sCipher.size() >= (sMsg.size() + crypto_aead_aes256gcm_ABYTES));
  sMsg2 = sodium->aead_aes256gcm_decrypt(sCipher, nonce, key);
  ASSERT_FALSE(sMsg2.empty());
  ASSERT_EQ(sMsg.size(), sMsg2.size());
  ASSERT_EQ(sMsg, sMsg2);

  // decrypt with additional data although AD was not provided
  // during encryption
  sMsg2 = sodium->aead_aes256gcm_decrypt(sCipher, nonce, key, sAd);
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
  sodium->genAsymCryptoKeyPair(pk, sk);

  // re-gen the public key from the secret key
  SodiumLib::AsymCrypto_PublicKey pk2;
  ASSERT_TRUE(sodium->genPublicCryptoKeyFromSecretKey(sk, pk2));
  ASSERT_TRUE(sodium->memcmp(pk.toMemView(), pk2.toMemView()));

  // gen key pair from seed
  SodiumLib::AsymCrypto_KeySeed seed;
  sodium->randombytes_buf(seed.toNotOwningArray());
  sodium->genAsymCryptoKeyPairSeeded(seed, pk, sk);
  SodiumLib::AsymCrypto_SecretKey sk2;
  sodium->genAsymCryptoKeyPairSeeded(seed, pk2, sk2);
  ASSERT_TRUE(sodium->memcmp(pk.toMemView(), pk2.toMemView()));
  ASSERT_TRUE(sodium->memcmp(sk.toMemView(), sk2.toMemView()));
}

//----------------------------------------------------------------------------

TEST(Sodium, AsymKeyCrypto_Buffer)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random key pair for sender and recipient
  SodiumLib::AsymCrypto_PublicKey pkSender;
  SodiumLib::AsymCrypto_SecretKey skSender;
  sodium->genAsymCryptoKeyPair(pkSender, skSender);
  SodiumLib::AsymCrypto_PublicKey pkRecipient;
  SodiumLib::AsymCrypto_SecretKey skRecipient;
  sodium->genAsymCryptoKeyPair(pkRecipient, skRecipient);

  // generate a random message
  static constexpr size_t msgSize = 500;
  MemArray msg{msgSize};
  sodium->randombytes_buf(msg);

  // generate a nonce
  SodiumLib::AsymCrypto_Nonce nonce{SodiumKeyInitStyle::Random};

  // encrypt a message
  auto cipher = sodium->box_easy(msg.view(), nonce, pkRecipient, skSender);
  ASSERT_FALSE(cipher.empty());
  ASSERT_EQ(msgSize + crypto_box_MACBYTES, cipher.size());

  // decrypt the message
  auto msg2 = sodium->box_open_easy(cipher.view(), nonce, pkSender, skRecipient);
  ASSERT_FALSE(msg2.empty());
  ASSERT_EQ(msgSize, msg2.size());
  ASSERT_TRUE(sodium->memcmp(msg.view(), msg2.toMemView()));

  // tamper with PK, SK, nonce and cipher
  for (unsigned char* ptr : {pkSender.to_ucPtr_rw(), skRecipient.to_ucPtr_rw(), nonce.to_ucPtr_rw(), cipher.to_ucPtr()})
  {
    ptr[5] += 1;
    msg2 = sodium->box_open_easy(cipher.view(), nonce, pkSender, skRecipient);
    ASSERT_TRUE(msg2.empty());
    ptr[5] -= 1;
    msg2 = sodium->box_open_easy(cipher.view(), nonce, pkSender, skRecipient);
    ASSERT_FALSE(msg2.empty());
    ASSERT_EQ(msgSize, msg2.size());
    ASSERT_TRUE(sodium->memcmp(msg.view(), msg2.toMemView()));
  }

  //
  // test the detached versions
  //

  SodiumLib::AsymCrypto_Tag mac;
  auto tmp = sodium->box_detached(msg.view(), nonce, pkRecipient, skSender);
  cipher = std::move(tmp.first);
  mac = std::move(tmp.second);
  ASSERT_FALSE(cipher.empty());
  ASSERT_EQ(msgSize, cipher.size());
  ASSERT_FALSE(mac.empty());
  ASSERT_EQ(crypto_box_MACBYTES, mac.size());

  msg2 = sodium->box_open_detached(cipher.view(), mac, nonce, pkSender, skRecipient);
  ASSERT_FALSE(msg2.empty());
  ASSERT_EQ(msgSize, msg2.size());
  ASSERT_TRUE(sodium->memcmp(msg.view(), msg2.toMemView()));

  // tamper with PK, SK, nonce, mac and ciper
  for (unsigned char* ptr : {pkSender.to_ucPtr_rw(), skRecipient.to_ucPtr_rw(), nonce.to_ucPtr_rw(), cipher.to_ucPtr(), mac.to_ucPtr_rw()})
  {
    ptr[5] += 1;
    msg2 = sodium->box_open_detached(cipher.view(), mac, nonce, pkSender, skRecipient);
    ASSERT_TRUE(msg2.empty());
    ptr[5] -= 1;
    msg2 = sodium->box_open_detached(cipher.view(), mac, nonce, pkSender, skRecipient);
    ASSERT_FALSE(msg2.empty());
    ASSERT_EQ(msgSize, msg2.size());
    ASSERT_TRUE(sodium->memcmp(msg.view(), msg2.toMemView()));
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
  sodium->genAsymCryptoKeyPair(pkSender, skSender);
  SodiumLib::AsymCrypto_PublicKey pkRecipient;
  SodiumLib::AsymCrypto_SecretKey skRecipient;
  sodium->genAsymCryptoKeyPair(pkRecipient, skRecipient);

  // generate a random message
  static constexpr size_t msgSize = 500;
  MemArray _msg{msgSize};
  sodium->randombytes_buf(_msg);
  string msg{_msg.to_charPtr(), msgSize};

  // generate a nonce
  SodiumLib::AsymCrypto_Nonce nonce{SodiumKeyInitStyle::Random};

  // encrypt a message
  string cipher = sodium->box_easy(msg, nonce, pkRecipient, skSender);
  ASSERT_FALSE(cipher.empty());
  ASSERT_EQ(msgSize + crypto_box_MACBYTES, cipher.size());

  // decrypt the message
  string msg2 = sodium->box_open_easy(cipher, nonce, pkSender, skRecipient);
  ASSERT_FALSE(msg2.empty());
  ASSERT_EQ(msgSize, msg2.size());
  ASSERT_EQ(msg, msg2);

  // tamper with PK, SK, mac, nonce and ciper
  for (unsigned char* ptr : {pkSender.to_ucPtr_rw(), skRecipient.to_ucPtr_rw(), nonce.to_ucPtr_rw(), (unsigned char *)cipher.c_str()})
  {
    ptr[5] += 1;
    msg2 = sodium->box_open_easy(cipher, nonce, pkSender, skRecipient);
    ASSERT_TRUE(msg2.empty());
    ptr[5] -= 1;
    msg2 = sodium->box_open_easy(cipher, nonce, pkSender, skRecipient);
    ASSERT_FALSE(msg2.empty());
    ASSERT_EQ(msgSize, msg2.size());
    ASSERT_EQ(msg, msg2);
  }

  //
  // test the detached versions
  //

  SodiumLib::AsymCrypto_Tag mac;
  auto tmp = sodium->box_detached(msg, nonce, pkRecipient, skSender);
  cipher = std::move(tmp.first);
  mac = std::move(tmp.second);
  ASSERT_FALSE(cipher.empty());
  ASSERT_EQ(msgSize, cipher.size());
  ASSERT_FALSE(mac.empty());
  ASSERT_EQ(crypto_box_MACBYTES, mac.size());

  msg2 = sodium->box_open_detached(cipher, mac, nonce, pkSender, skRecipient);
  ASSERT_FALSE(msg2.empty());
  ASSERT_EQ(msgSize, msg2.size());
  ASSERT_EQ(msg, msg2);

  // tamper with PK, SK, nonce, mac and ciper
  for (unsigned char* ptr : {pkSender.to_ucPtr_rw(), skRecipient.to_ucPtr_rw(), nonce.to_ucPtr_rw(), mac.to_ucPtr_rw(), (unsigned char *)cipher.c_str()})
  {
    ptr[5] += 1;
    msg2 = sodium->box_open_detached(cipher, mac, nonce, pkSender, skRecipient);
    ASSERT_TRUE(msg2.empty());
    ptr[5] -= 1;
    msg2 = sodium->box_open_detached(cipher, mac, nonce, pkSender, skRecipient);
    ASSERT_FALSE(msg2.empty());
    ASSERT_EQ(msgSize, msg2.size());
    ASSERT_EQ(msg, msg2);
  }
}

//----------------------------------------------------------------------------

TEST(Sodium, AsymKeyHandling_Sign)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random key pair
  SodiumLib::AsymSign_PublicKey pk;
  SodiumLib::AsymSign_SecretKey sk;
  sodium->genAsymSignKeyPair(pk, sk);

  // re-gen the public key from the secret key
  SodiumLib::AsymSign_PublicKey pk2;
  ASSERT_TRUE(sodium->genPublicSignKeyFromSecretKey(sk, pk2));
  ASSERT_TRUE(sodium->memcmp(pk.toMemView(), pk2.toMemView()));

  // re-gen the seed from the secret key
  SodiumLib::AsymSign_KeySeed seed;
  ASSERT_TRUE(sodium->genSignKeySeedFromSecretKey(sk, seed));

  // gen key pair from seed
  SodiumLib::AsymSign_SecretKey sk2;
  sodium->genAsymSignKeyPairSeeded(seed, pk2, sk2);
  ASSERT_TRUE(sodium->memcmp(pk.toMemView(), pk2.toMemView()));
  ASSERT_TRUE(sodium->memcmp(sk.toMemView(), sk2.toMemView()));
}

//----------------------------------------------------------------------------

TEST(Sodium, AsymKeySign_Buffer)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random key pair
  SodiumLib::AsymSign_PublicKey pk;
  SodiumLib::AsymSign_SecretKey sk;
  sodium->genAsymSignKeyPair(pk, sk);

  // generate a random message
  static constexpr size_t msgSize = 500;
  MemArray msg{msgSize};
  sodium->randombytes_buf(msg);

  // sign the message
  auto signedMsg = sodium->sign(msg.view(), sk);
  ASSERT_EQ(msgSize + crypto_sign_BYTES, signedMsg.size());

  // check and remove the signature
  auto msg2 = sodium->sign_open(signedMsg.view(), pk);
  ASSERT_EQ(msgSize, msg2.size());
  ASSERT_TRUE(sodium->memcmp(msg.view(), msg2.view()));

  // tamper with message and public key
  for (unsigned char* ptr : {signedMsg.to_uint8Ptr(), pk.to_ucPtr_rw()})
  {
    ptr[5] += 1;
    msg2 = sodium->sign_open(signedMsg.view(), pk);
    ASSERT_TRUE(msg2.empty());
    ptr[5] -= 1;
    msg2 = sodium->sign_open(signedMsg.view(), pk);
    ASSERT_FALSE(msg2.empty());
    ASSERT_EQ(msgSize, msg2.size());
    ASSERT_TRUE(sodium->memcmp(msg.view(), msg2.view()));
  }

  //
  // detached version
  //

  SodiumLib::AsymSign_Signature sig = sodium->sign_detached(msg.view(), sk);
  ASSERT_TRUE(sodium->sign_verify_detached(msg.view(), sig, pk));

  // tamper with message, signature and public key
  for (unsigned char* ptr : {msg.to_uint8Ptr(), sig.to_ucPtr_rw(), pk.to_ucPtr_rw()})
  {
    ptr[5] += 1;
    ASSERT_FALSE(sodium->sign_verify_detached(msg.view(), sig, pk));
    ptr[5] -= 1;
    ASSERT_TRUE(sodium->sign_verify_detached(msg.view(), sig, pk));
  }
}

//----------------------------------------------------------------------------

TEST(Sodium, AsymKeySign_String)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random key pair
  SodiumLib::AsymSign_PublicKey pk;
  SodiumLib::AsymSign_SecretKey sk;
  sodium->genAsymSignKeyPair(pk, sk);

  // generate a random message
  static constexpr size_t msgSize = 500;
  MemArray _msg{msgSize};
  sodium->randombytes_buf(_msg);
  string msg{_msg.to_charPtr(), msgSize};

  // sign the message
  string signedMsg = sodium->sign(msg, sk);
  ASSERT_FALSE(signedMsg.empty());
  ASSERT_EQ(msgSize + crypto_sign_BYTES, signedMsg.size());

  // check and remove the signature
  string msg2 = sodium->sign_open(signedMsg, pk);
  ASSERT_FALSE(msg2.empty());
  ASSERT_EQ(msgSize, msg2.size());
  ASSERT_EQ(msg, msg2);

  // tamper with message and public key
  for (unsigned char* ptr : {(unsigned char *)signedMsg.c_str(), pk.to_ucPtr_rw()})
  {
    ptr[5] += 1;
    msg2 = sodium->sign_open(signedMsg, pk);
    ASSERT_TRUE(msg2.empty());
    ptr[5] -= 1;
    msg2 = sodium->sign_open(signedMsg, pk);
    ASSERT_FALSE(msg2.empty());
    ASSERT_EQ(msgSize, msg2.size());
    ASSERT_EQ(msg, msg2);
  }

  //
  // detached version
  //

  SodiumLib::AsymSign_Signature sig = sodium->sign_detached(msg, sk);
  ASSERT_FALSE(sig.empty());
  ASSERT_TRUE(sodium->sign_verify_detached(msg, sig, pk));

  // tamper with message, signature and public key
  for (unsigned char* ptr : {(unsigned char *)msg.c_str(), sig.to_ucPtr_rw(), pk.to_ucPtr_rw()})
  {
    ptr[5] += 1;
    ASSERT_FALSE(sodium->sign_verify_detached(msg, sig, pk));
    ptr[5] -= 1;
    ASSERT_TRUE(sodium->sign_verify_detached(msg, sig, pk));
  }
}

//----------------------------------------------------------------------------

TEST(Sodium, GenericHashing_Buffer)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random hashing key
  SodiumLib::GenericHashKey k{SodiumKeyInitStyle::Random};

  // generate a random message
  static constexpr size_t msgSize = 500;
  MemArray msg{msgSize};
  sodium->randombytes_buf(msg);

  // hash without key
  auto h1 = sodium->generichash(msg.view());
  ASSERT_FALSE(h1.empty());
  msg[10] += 1;
  auto h1a = sodium->generichash(msg.view());
  ASSERT_FALSE(h1a.empty());
  ASSERT_EQ(h1.size(), h1a.size());
  ASSERT_FALSE(sodium->memcmp(h1.view(), h1a.view()));
  msg[10] -= 1;
  h1a = sodium->generichash(msg.view());
  ASSERT_FALSE(h1a.empty());
  ASSERT_EQ(h1.size(), h1a.size());
  ASSERT_TRUE(sodium->memcmp(h1.view(), h1a.view()));

  // hash with key
  auto h2 = sodium->generichash(msg.view(), k);
  ASSERT_FALSE(h2.empty());
  msg[10] += 1;
  auto h2a = sodium->generichash(msg.view(), k);
  ASSERT_FALSE(h2a.empty());
  ASSERT_EQ(h2.size(), h2a.size());
  ASSERT_FALSE(sodium->memcmp(h2.view(), h2a.view()));
  msg[10] -= 1;
  h2a = sodium->generichash(msg.view(), k);
  ASSERT_FALSE(h2a.empty());
  ASSERT_EQ(h2.size(), h2a.size());
  ASSERT_TRUE(sodium->memcmp(h2.view(), h2a.view()));

  // use the hasher class
  GenericHasher gh;
  gh.append(msg.view());
  auto h3 = gh.finalize();
  ASSERT_FALSE(h3.empty());
  ASSERT_TRUE(sodium->memcmp(h1.view(), h3.view()));

  GenericHasher gh2{k};
  gh2.append(msg.view());
  auto h4 = gh2.finalize();
  ASSERT_FALSE(h4.empty());
  ASSERT_TRUE(sodium->memcmp(h2.view(), h4.view()));

  // make sure we can't use the class anymore after
  // we've called finalize
  h3 = gh.finalize();
  ASSERT_TRUE(h3.empty());
  h4 = gh2.finalize();
  ASSERT_TRUE(h4.empty());
}

//----------------------------------------------------------------------------

TEST(Sodium, GenericHashing_String)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random hashing key
  SodiumLib::GenericHashKey k{SodiumKeyInitStyle::Random};

  // generate a random message
  static constexpr size_t msgSize = 500;
  MemArray _msg{msgSize};
  sodium->randombytes_buf(_msg);
  string msg{_msg.to_charPtr(), msgSize};

  // hash without key
  string h1 = sodium->generichash(msg);
  ASSERT_FALSE(h1.empty());

  // hash with key
  string h2 = sodium->generichash(msg, k);
  ASSERT_FALSE(h2.empty());
  ASSERT_FALSE(h1 == h2);

  // use the hasher class
  GenericHasher gh;
  gh.append(msg);
  string h3 = gh.finalize_string();
  ASSERT_FALSE(h3.empty());
  ASSERT_EQ(h1, h3);

  GenericHasher gh2{k};
  gh2.append(msg);
  string h4 = gh2.finalize_string();
  ASSERT_FALSE(h4.empty());
  ASSERT_EQ(h2, h4);

  // make sure we can't use the class anymore after
  // we've called finalize
  h3 = gh.finalize_string();
  ASSERT_TRUE(h3.empty());
  h4 = gh2.finalize_string();
  ASSERT_TRUE(h4.empty());
}

//----------------------------------------------------------------------------

TEST(Sodium, ShortHash)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate a random hashing key
  SodiumLib::ShorthashKey k{SodiumKeyInitStyle::Random};

  // generate a random message
  static constexpr size_t msgSize = 500;
  MemArray msg{msgSize};
  sodium->randombytes_buf(msg);
  string sMsg{msg.to_charPtr(), msgSize};

  // shorthash using buffers
  auto bufHash = sodium->shorthash(msg.view(), k);
  ASSERT_FALSE(bufHash.empty());

  // shorthash using strings
  string sHash = sodium->shorthash(sMsg, k);
  ASSERT_FALSE(sHash.empty());

  // cross-compare results
  MemView sView{sHash};
  ASSERT_TRUE(sodium->memcmp(sView, bufHash.view()));
}

//----------------------------------------------------------------------------

TEST(Sodium, PasswdHash)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // a random passwd
  static constexpr size_t pwSize = 20;
  MemArray pw{pwSize};
  sodium->randombytes_buf(pw);

  // create a 16-byte hash from it
  static constexpr size_t hashLen = 16;
  auto tmp = sodium->pwhash(pw.view(), hashLen);
  SodiumSecureMemory hash;
  SodiumLib::PwHashData hDat;
  hash = std::move(tmp.first);
  hDat = std::move(tmp.second);
  ASSERT_FALSE(hash.empty());
  ASSERT_EQ(hashLen, hash.size());
  ASSERT_FALSE(hDat.salt.empty());
  cout << "Moderate opslimit for Argon2 is " << hDat.opslimit << endl;
  cout << "Moderate memlimit for Argon2 is " << hDat.memlimit << endl;

  // make sure the hash is reproducible with
  // the parameters provided in hDat
  string oldSalt{hDat.salt.toMemView().to_charPtr(), hDat.salt.size()};
  SodiumSecureMemory hash2 = sodium->pwhash(pw.view(), hashLen, hDat);
  ASSERT_FALSE(hash2.empty());
  ASSERT_EQ(hashLen, hash2.size());
  ASSERT_TRUE(sodium->memcmp(hash.toMemView(), hash2.toMemView()));
  string saltAfterCall{hDat.salt.toMemView().to_charPtr(), hDat.salt.size()};
  ASSERT_EQ(oldSalt, saltAfterCall);
}

//----------------------------------------------------------------------------

TEST(Sodium, PasswdHashStr)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // a random password
  string pw{"This is some password"};

  // hash it
  string hash = sodium->pwhash_str(pw);
  ASSERT_FALSE(hash.empty());
  cout << "The password hash is: " << hash << endl;

  // verify it with a valid PW
  ASSERT_TRUE(sodium->pwhash_str_verify(pw, hash));

  // try to verify a wrong pw
  ASSERT_FALSE(sodium->pwhash_str_verify("xyz", hash));
}

//----------------------------------------------------------------------------

TEST(Sodium, DiffieHellmann)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  DiffieHellmannExchanger c{true};
  DiffieHellmannExchanger s{false};

  auto shared1 = c.getSharedSecret(s.getMyPublicKey());
  auto shared2 = s.getSharedSecret(c.getMyPublicKey());

  ASSERT_FALSE(shared1.empty());
  ASSERT_FALSE(shared2.empty());
  ASSERT_TRUE(sodium->memcmp(shared1.toMemView(), shared2.toMemView()));

  //cout << "Shared secret is: " << toBase64(shared1) << endl;
  //cout << "Shared secret length is: " << shared1.size() * 8 << " bit" << endl;

  // tamper with a public key
  auto pkServ = s.getMyPublicKey();
  unsigned char* ptr = pkServ.to_ucPtr_rw();
  ptr[2] += 1;
  shared1 = c.getSharedSecret(pkServ);
  ASSERT_FALSE(sodium->memcmp(shared1.toMemView(), shared2.toMemView()));
  //cout << "Wrong shared secret is: " << toBase64(shared1) << endl;

  ptr[2] -= 1;
  shared1 = c.getSharedSecret(pkServ);
  ASSERT_TRUE(sodium->memcmp(shared1.toMemView(), shared2.toMemView()));

}

//----------------------------------------------------------------------------

TEST(Sodium, CryptoKx_FullProcess)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // generate one random key pair for each client and server
  auto srvPair = sodium->genKeyExchangeKeyPair();
  auto cliPair = sodium->genKeyExchangeKeyPair();

  // compute session keys rx and tx for both client and server
  auto srvSession = sodium->getServerSessionKeys(srvPair.second, srvPair.first, cliPair.second);
  auto cliSession = sodium->getClientSessionKeys(cliPair.second, cliPair.first, srvPair.second);

  // compare rx/tx pairs of client and server;
  // client's rx must match server's tx and vice versa
  ASSERT_TRUE(sodium->memcmp(srvSession.first.toMemView(), cliSession.second.toMemView()));
  ASSERT_TRUE(sodium->memcmp(srvSession.second.toMemView(), cliSession.first.toMemView()));
}

//----------------------------------------------------------------------------

TEST(Sodium, CryptoKx_SeededKeys)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  SodiumLib::KX_KeySeed seed{SodiumKeyInitStyle::Random};
  auto pair1 = sodium->genKeyExchangeKeyPair(seed);
  auto pair2 = sodium->genKeyExchangeKeyPair(seed);

  ASSERT_TRUE(sodium->memcmp(pair1.first.toMemView(), pair2.first.toMemView()));
  ASSERT_TRUE(sodium->memcmp(pair1.second.toMemView(), pair2.second.toMemView()));
}

//----------------------------------------------------------------------------

TEST(Sodium, DiffieHellmann2)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  DiffieHellmannExchanger2 c{true};
  DiffieHellmannExchanger2 s{false};

  auto shared1 = c.getSessionKeys(s.getMyPublicKey());
  auto shared2 = s.getSessionKeys(c.getMyPublicKey());

  ASSERT_FALSE(shared1.first.empty());
  ASSERT_FALSE(shared1.second.empty());
  ASSERT_FALSE(shared2.first.empty());
  ASSERT_FALSE(shared2.second.empty());
  ASSERT_TRUE(sodium->memcmp(shared1.first.toMemView(), shared2.second.toMemView()));
  ASSERT_TRUE(sodium->memcmp(shared1.second.toMemView(), shared2.first.toMemView()));

  // tamper with a public key
  SodiumLib::KX_PublicKey pkServ = s.getMyPublicKey();
  unsigned char* ptr = pkServ.to_ucPtr_rw();
  ptr[2] += 1;
  shared1 = c.getSessionKeys(pkServ);
  ASSERT_FALSE(sodium->memcmp(shared1.first.toMemView(), shared2.second.toMemView()));
  ASSERT_FALSE(sodium->memcmp(shared1.second.toMemView(), shared2.first.toMemView()));

  ptr[2] -= 1;
  shared1 = c.getSessionKeys(pkServ);
  ASSERT_TRUE(sodium->memcmp(shared1.first.toMemView(), shared2.second.toMemView()));
  ASSERT_TRUE(sodium->memcmp(shared1.second.toMemView(), shared2.first.toMemView()));
}


//----------------------------------------------------------------------------

TEST(Sodium, Base64)
{
  SodiumLib* sodium = SodiumLib::getInstance();
  ASSERT_TRUE(sodium != nullptr);

  // test encoding and decoding of random buffers
  // with 1 to 16 bytes
  for (size_t s = 1; s <= 16; ++s)
  {
    Sloppy::MemArray inData{s};
    sodium->randombytes_buf(inData);
    string inString{inData.to_charPtr(), inData.size()};
    ASSERT_EQ(s, inString.size());

    // encoding
    Sloppy::MemArray b64 = sodium->bin2Base64(inData.view());
    string b64String = sodium->bin2Base64(inString);

    // decoding
    Sloppy::MemArray decodedMem = sodium->base642Bin(b64.view());
    string decodedString = sodium->base642Bin(b64String);

    // compare
    ASSERT_EQ(s, decodedString.size());
    ASSERT_EQ(s, decodedMem.size());
    ASSERT_TRUE(sodium->memcmp(inData.view(), decodedMem.view()));
    ASSERT_EQ(decodedString, inString);
  }
}
