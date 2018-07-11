#!/bin/bash

echo
echo Retrieving a copy of the latest version of
echo "Niels Lohmann's header-only JSON implementation"
echo and committing it to the repository
echo

curl https://raw.githubusercontent.com/boostorg/date_time/master/data/date_time_zonespec.csv > date_time_zonespec.csv

if [ $? -ne 0 ]; then
  echo
  echo "!!! Download failed !!!"
  echo
  exit 1
fi

git add date_time_zonespec.csv
git commit -m "Update of date_time_zonespec.csv from Github" date_time_zonespec.csv

