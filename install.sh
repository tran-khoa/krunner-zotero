#!/bin/bash

set -e

mkdir -p build
cd build
krunner_version=$(krunner --version | grep -oP "(?<=krunner )\d+")

cmake .. -DCMAKE_BUILD_TYPE=Release -DKDE_INSTALL_USE_QT_SYS_PATHS=ON -DBUILD_TESTING=OFF -DBUILD_WITH_QT6=ON
make -j$(nproc)
sudo make install

if pgrep -x "krunner" > /dev/null
then
    kquitapp"$krunner_version" krunner
fi

echo "Installation finished"