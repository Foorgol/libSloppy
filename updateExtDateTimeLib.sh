#!/bin/bash


echo
echo Retrieving the latest master version of
echo "Howard Hinnant's date / time library"
echo and committing it to the repository
echo

cd Sloppy/DateTime
if [ $? -ne 0 ]; then
  echo
  echo "Please call this script from the local repository's"
  echo "root directory"
  echo

  exit 1 
fi

# download the header files
for f in date.h tz.h tz_private.h; do
  curl https://raw.githubusercontent.com/HowardHinnant/date/master/include/date/$f > $f
  if [ $? -ne 0 ]; then
    echo
    echo "!!! Download of $f failed !!!"
    echo
    exit 1
  fi
done

# download the source files
for f in tz.cpp; do
  curl https://raw.githubusercontent.com/HowardHinnant/date/master/src/$f > $f
  if [ $? -ne 0 ]; then
    echo
    echo "!!! Download of $f failed !!!"
    echo
    exit 1
  fi
done

# patch the source file to include headers from the current directory
# and not from a "date" subdirectory
sed -i -- 's/include "date\/tz_private.h"/include "tz_private.h"/' tz.cpp


git add tz.cpp date.h tz.h tz_private.h
git commit -m "Update of external date/time lib from Github" tz.cpp date.h tz.h tz_private.h

