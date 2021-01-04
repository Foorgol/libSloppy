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

#include <iostream>

#include <gtest/gtest.h>

#include "../Sloppy/Crypto/Sodium.h"

using namespace Sloppy::Crypto;
using namespace std;

TEST(PasswordProtectedSecr, ctor)
{
  // the most trivial ctor
  PasswordProtectedSecret pps;
  ASSERT_FALSE(pps.hasContent());
  ASSERT_FALSE(pps.hasPassword());
  ASSERT_THROW(pps.setSecret("sdkfsdf"), PasswordProtectedSecret::NoPasswordSet);  // has no PW set
  ASSERT_THROW(pps.getSecretAsString(), PasswordProtectedSecret::NoPasswordSet);
  ASSERT_THROW(pps.changePassword("", "sdkfsdf"), PasswordProtectedSecret::NoPasswordSet);

  // ctor with data
  string dat = pps.asString();
  pps = PasswordProtectedSecret(dat, false);
  ASSERT_FALSE(pps.hasContent());
  ASSERT_FALSE(pps.hasPassword());
}

//----------------------------------------------------------------------------

TEST(PasswordProtectedSecr, readWrite1)
{
  // create an empty container
  PasswordProtectedSecret pps;

  // try to store data although no pw is set
  ASSERT_THROW(pps.setSecret("secret"), PasswordProtectedSecret::NoPasswordSet);

  // set a password
  ASSERT_TRUE(pps.setPassword("abc123"));
  ASSERT_TRUE(pps.hasPassword());
  ASSERT_TRUE(pps.isValidPassword("abc123"));

  // store a secret
  ASSERT_TRUE(pps.setSecret("secret"));

  // read back
  ASSERT_EQ("secret", pps.getSecretAsString());

  // read from an empty container
  pps = PasswordProtectedSecret{};
  ASSERT_THROW(pps.getSecretAsString(), PasswordProtectedSecret::NoPasswordSet);
  pps.setPassword("abc123");
  ASSERT_EQ("", pps.getSecretAsString());

  // change PW of an empty container
  ASSERT_TRUE(pps.changePassword("abc123", "qqq666"));
  ASSERT_FALSE(pps.isValidPassword("abc123"));
  ASSERT_TRUE(pps.isValidPassword("qqq666"));

  // store content
  ASSERT_FALSE(pps.hasContent());
  ASSERT_TRUE(pps.setSecret("secret"));
  ASSERT_TRUE(pps.hasContent());
  ASSERT_EQ("secret", pps.getSecretAsString());

  // change the PW
  ASSERT_FALSE(pps.changePassword("kfhsf", "dajkfsdf"));
  ASSERT_TRUE(pps.isValidPassword("qqq666"));
  ASSERT_EQ("secret", pps.getSecretAsString());

  ASSERT_FALSE(pps.changePassword("qqq666", ""));
  ASSERT_TRUE(pps.isValidPassword("qqq666"));
  ASSERT_EQ("secret", pps.getSecretAsString());

  ASSERT_TRUE(pps.changePassword("qqq666", "abc123"));
  ASSERT_FALSE(pps.isValidPassword("qqq666"));
  ASSERT_TRUE(pps.isValidPassword("abc123"));
  ASSERT_EQ("secret", pps.getSecretAsString());
}

//----------------------------------------------------------------------------

TEST(PasswordProtectedSecr, ExportImport)
{
  // create an empty container
  PasswordProtectedSecret pps;
  ASSERT_TRUE(pps.setPassword("abc123"));

  // set some content
  ASSERT_TRUE(pps.setSecret("secret"));
  ASSERT_TRUE(pps.hasContent());
  ASSERT_EQ("secret", pps.getSecretAsString());
  string dat = pps.asString();
  ASSERT_FALSE(dat.empty());

  // re-import
  PasswordProtectedSecret pps1(dat, false);
  ASSERT_TRUE(pps1.hasContent());
  ASSERT_THROW(pps1.getSecretAsString(), PasswordProtectedSecret::NoPasswordSet);
  ASSERT_FALSE(pps1.setPassword("sdkfjsdf"));
  ASSERT_FALSE(pps1.setPassword(""));
  ASSERT_TRUE(pps1.setPassword("abc123"));
  ASSERT_EQ("secret", pps1.getSecretAsString());
}
