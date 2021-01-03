/*
 *    This is libSloppy, a library of sloppily implemented helper functions.
 *    Copyright (C) 2016 - 2019  Volker Knollmann
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

#include "BasicTestClass.h"

void BasicTestFixture::SetUp()
{
  log = unique_ptr<Sloppy::Logger::Logger>(new Sloppy::Logger::Logger("UnitTest"));

  // create a dir for temporary files created during testing
  tstDirPath = std::filesystem::temp_directory_path();
  if (!(std::filesystem::exists(tstDirPath)))
  {
    throw std::runtime_error("Could not create temporary directory for test files!");
  }

  log->log("Using directory " + tstDirPath.native() + " for temporary files");
}

void BasicTestFixture::TearDown()
{
//  QString path = tstDir.path();

//  if (!(tstDir.remove()))
//  {
//    QString msg = "Could not remove temporary directory " + path;
//    QByteArray ba = msg.toLocal8Bit();
//    throw std::runtime_error(ba.data());
//  }

//  log.info("Deleted temporary directory " + tstDir.path() + " and all its contents");
}

string BasicTestFixture::getTestDir() const
{
  return tstDirPath.native();
}

string BasicTestFixture::genTestFilePath(string fName) const
{
  std::filesystem::path p = tstDirPath;
  p /= fName;
  return p.native();
}

