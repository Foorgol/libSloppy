#!/bin/bash

echo
echo Retrieving a copy of the latest version of
echo "Niels Lohmann's header-only JSON implementation"
echo and committing it to the repository
echo

curl https://raw.githubusercontent.com/nlohmann/json/master/single_include/nlohmann/json.hpp > json.hpp
if [ $? -ne 0 ]; then
  echo
  echo "!!! Download failed !!!"
  echo
  exit 1
fi

curl https://raw.githubusercontent.com/nlohmann/json/master/include/nlohmann/json_fwd.hpp > json_fwd.hpp
if [ $? -ne 0 ]; then
  echo
  echo "!!! Download failed !!!"
  echo
  exit 1
fi

git add json.hpp json_fwd.hpp
git commit -m "Update of json.hpp from Github" json.hpp json_fwd.hpp
