#!/bin/bash

if [ $0 != "./releaseNewVersion.sh" ]; then
  echo
  echo "Please call the script from the repository's root directory"
  echo
  exit 1
fi

# make sure we're on branch "dev"
out=$(git branch --show-current)
if [ $out != "dev" ]; then
  echo
  echo "Must be on branch 'dev'"
  echo
  exit 1
fi

# make sure there are no pending changes
#
# assumption: if there are any fragments with string length
# "1" or "2" and it is not "??", we assume that it indicates
# a pending change (e.g., "A", "AM", "D", ...)
for tag in $(git status --porcelain); do
  tagLen=$(expr length $tag)
  if [ $tagLen -lt 3 -a $tag != "??" ]; then
    echo
    echo "Work dir not clean; all changes must have been committed!"
    echo
    exit 1
  fi
done

# show the most recent version numbers before asking
# for the new version
echo
echo "List of the most recent tags:"
echo
git tag | tail

# ask for the version number
echo
echo "New major version number:"
echo -n "--> "
read VER_MA
echo
echo "New minor version number:"
echo -n "--> "
read VER_MI
echo
echo "New patch number:"
echo -n "--> "
read VER_PA

VER_FULL=$VER_MA"."$VER_MI"."$VER_PA

echo
echo "The new tag will be '$VER_FULL'"
echo "--> Enter = proceed, Ctrl-C = cancel"
read

# create a backup of the CMakeLists.txt file
cp CMakeLists.txt CMakeLists.txt~

# replace the version numbers
sed -i -- "s/LIB_VERSION_MAJOR [[:digit:]][[:digit:]]*/LIB_VERSION_MAJOR $VER_MA/" CMakeLists.txt
sed -i -- "s/LIB_VERSION_MINOR [[:digit:]][[:digit:]]*/LIB_VERSION_MINOR $VER_MI/" CMakeLists.txt
sed -i -- "s/LIB_VERSION_PATCH [[:digit:]][[:digit:]]*/LIB_VERSION_PATCH $VER_PA/" CMakeLists.txt

# update the PKGBUILD file
pushd dist
cp PKGBUILD PKGBUILD~
sed -i -- "s/pkgver=.*/pkgver=$VER_FULL/" PKGBUILD
popd

# commit the version changes
git commit -m "Release of version $VER_FULL" CMakeLists.txt dist/PKGBUILD
if [ $? -ne 0 ]; then
  echo
  echo "'git commit' failed!"
  echo
  exit 1
fi

# switch to master and merge all changes
echo
echo "Ready to merge all changes from dev with master?"
echo "--> Enter = proceed, Ctrl-C = cancel"
read

git checkout master
if [ $? -ne 0 ]; then
  echo
  echo "Changing the current branch to 'master' failed"
  echo
  exit 1
fi

git merge dev
if [ $? -ne 0 ]; then
  echo
  echo "Merging 'master' and 'dev' failed"
  echo
  exit 1
fi

# create the tag
git tag $VER_FULL
if [ $? -ne 0 ]; then
  echo
  echo "'git tag' failed!"
  echo
  exit 1
fi

# change to the dev branch to prepage for the next modifications
git checkout dev

echo
echo "!!! All good ; switched back to DEV BRANCH !!!"
echo

exit 0

