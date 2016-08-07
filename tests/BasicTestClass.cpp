#include <boost/filesystem.hpp>
#include <boost/log/trivial.hpp>

#include "BasicTestClass.h"

namespace boostfs = boost::filesystem;

void BasicTestFixture::SetUp()
{
  log = unique_ptr<Sloppy::Logger::Logger>(new Sloppy::Logger::Logger("UnitTest"));

  // create a dir for temporary files created during testing
  tstDirPath = boostfs::temp_directory_path();
  if (!(boostfs::exists(tstDirPath)))
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
  boostfs::path p = tstDirPath;
  p /= fName;
  return p.native();
}

void BasicTestFixture::printStartMsg(string _tcName)
{
  tcName = _tcName;
  log->trace("\n\n----------- Starting test case '" + tcName + "' -----------");
}

void BasicTestFixture::printEndMsg()
{
  log->trace("----------- End test case '" + tcName + "' -----------\n\n");
}
