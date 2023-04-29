#!/bin/bash
#
# Copyright (c) 2017 The Altra64 project contributors
# See LICENSE file in the project root for full license information.
#

set -euxo pipefail

# change to the users root directory
cd $HOME

#wget -c https://github.com/anacierdem/libdragon-docker/releases/download/v10.2.1/libdragon-linux-x86_64.tar.gz -O - | tar -xz

# Pull the latest libdragon source code and make a build directory
git clone https://github.com/dragonminded/libdragon.git
# set to correct commit
cd libdragon && git checkout --force b26fce6 && cd ..

# fix issues with the build scripts
sed -i -- 's|${N64_INST:-/usr/local}|/usr/local/libdragon|g' libdragon/tools/build
sed -i -- 's|--with-newlib|--with-newlib --with-system-zlib|g' libdragon/tools/build

sed -i -- 's| -lpng|\nLDLIBS = -lpng|g' libdragon/tools/mksprite/Makefile
sed -i -- 's| -Werror| -w|g' libdragon/tools/mksprite/Makefile

# make a build folder for libdragon
mkdir libdragon/build_gcc
cp libdragon/tools/build libdragon/build_gcc

# run the build script (this will take a while! and if not sudo, will ask for password mid flow!)
cd libdragon/build_gcc
./build

cd ..
# run the install script [sudo req]
make
make install
make tools
make tools-install

cd ..
# install libmikmod (custom version)
git clone https://github.com/n64-tools/libmikmod
cd libmikmod/n64
make
make install
cd .. # we have are in a subfolder, this is not a duplicate...

cd ..
# install libyaml
git clone https://github.com/yaml/libyaml
cd libyaml
./bootstrap
#$(N64_INST) converterd to $N64_INST below otherwise it will not run on WSFL
export PATH=$PATH:$N64_INST/bin
CFLAGS="-std=gnu99 -march=vr4300 -mtune=vr4300" \
LDFLAGS="-L$N64_INST/lib -Tn64ld.x" \
LIBS="-ldragon -lc -ldragonsys -lnosys" \
./configure --host=mips64-elf --prefix=$N64_INST
make
make install

cd ..
# install libmad (custom version)
git clone https://github.com/n64-tools/libmad
cd libmad
export PATH=$PATH:$N64_INST/bin
CFLAGS="-std=gnu99 -march=vr4300 -mtune=vr4300" \
LDFLAGS="-L$N64_INST/lib -Tn64ld.x" \
LIBS="-ldragon -lc -ldragonsys -lnosys" \
./configure --host=mips64-elf --prefix=$N64_INST
make
make install

cd ..

find /usr/local/libdragon/bin /usr/local/libdragon/mips64-elf/bin /usr/local/libdragon/libexec/gcc/mips64-elf -type f -print0 | xargs -0 strip || true
