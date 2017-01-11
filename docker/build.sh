#!/bin/bash
BUILD_ROOT=/root/build

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

# build release
cd $BUILD_ROOT/source-copy
spank --jobs 4 rebuild release

# copy back the artifacts to the mounted directory
cp $ARTIFACTS ../source
