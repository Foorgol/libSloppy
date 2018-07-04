#!/bin/bash

echo
echo Retrieving a copy of the latest version of
echo "Niels Lohmann's header-only JSON implementation"
echo and committing it to the repository
echo

curl https://raw.githubusercontent.com/nlohmann/json/develop/single_include/nlohmann/json.hpp
if [ $? -ne 0 ]; then
  echo
  echo "!!! Download failed !!!"
  echo
  exit 1
fi

git commit -m "Update of json.hpp from Github" json.hpp
