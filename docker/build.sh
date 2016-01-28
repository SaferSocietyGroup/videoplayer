#!/bin/bash
BUILD_ROOT=/root/build

SPANK_REPO=https://github.com/noname22/spank.git
SPANK=$BUILD_ROOT/spank/spank

ARTIFACTS="SSG.VideoPlayer.exe"

set -e

# set up build directory
mkdir -p $BUILD_ROOT

# make a copy of the source tree and build there
# because strip fails in docker on windows (virtualbox shared folders)
# https://www.virtualbox.org/ticket/8463

cd $BUILD_ROOT
cp -r source source-copy
cd source-copy

# clone and build spank
git clone $SPANK_REPO $BUILD_ROOT/spank
cd $BUILD_ROOT/spank
./build.sh

# build dependencies
cd $BUILD_ROOT/source-copy/deps
./deps.sh all 32bit

# build release
cd $BUILD_ROOT/source-copy
$SPANK --jobs 4 rebuild release

# copy back the artifacts to the mounted directory
cp $ARTIFACTS ../source
