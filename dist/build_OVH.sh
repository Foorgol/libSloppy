#!/bin/bash

PKGVER=0.1.3
PKGRELEASE=1
PKGNAME=libSloppy
BUILDDIR=/tmp/$PKGNAME-build

STARTDIR=$(pwd)

mkdir $BUILDDIR
pushd $BUILDDIR

git clone -b $PKGVER --single-branch --depth 1 file:///srv/git/libSloppy.git
cd libSloppy
mkdir release
cd release
cmake .. -DCMAKE_BUILD_TYPE=Release
make
echo "A C++ lib of sloppyly implemented helper functions" > description-pak
checkinstall --pkgversion $PKGVER \
	--pkgrelease $PKGRELEASE \
	--pkgname $PKGNAME \
	--provides $PKGNAME \
	--install=no \
	--nodoc \
	-y

mv *deb $STARTDIR

popd

rm -rf $BUILDDIR

