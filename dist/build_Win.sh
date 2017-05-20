#!/bin/bash

# define where to build the files
BUILD_DIR=/tmp/buildLibSloppy

#define the version to build
BUILD_VERSION=0.2.1

# the base path of the build tool binaries
MINGW64_BIN=/d/PortablePrograms/msys64/mingw64/bin

#
# No changes below this point!
#

REPO_BASE_URL=https://github.com/Foorgol/libsloppy/archive

PATH=$PATH:$MINGW64_BIN

export CXX=g++.exe
export CC=gcc.exe
CMAKE_GENERATOR=MSYS\ Makefiles
CMAKE_BIN=/d/PortablePrograms/msys64/mingw64/bin/cmake.exe

# create the build dir
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# make sure we are in the right directory
ACTUAL_DIR=$(pwd)
if [ $ACTUAL_DIR != $BUILD_DIR ]; then
	echo Could not enter build directory!
	exit 1
fi

#rm -rf *

# download sources
#wget $REPO_BASE_URL/$BUILD_VERSION.zip -O libSloppy-$BUILD_VERSION.zip

# Unzip and build the lib
#unzip libSloppy-$BUILD_VERSION.zip
#mv libSloppy-$BUILD_VERSION libSloppy 
cd libSloppy
mkdir release
cd release
$CMAKE_BIN -DCMAKE_BUILD_TYPE=Release -G "$CMAKE_GENERATOR" ..
make
cd ..
