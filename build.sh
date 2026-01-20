#!/bin/sh

set -ex

git clean -dxf

./bootstrap.sh > dev/logs/bootstrap.sh.log 2>&1
./configure > dev/logs/configure.log 2>&1
make -j4 > dev/logs/make.log 2>&1
sudo make install > dev/logs/make-install.log 2>&1