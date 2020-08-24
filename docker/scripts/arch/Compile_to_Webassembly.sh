#!/usr/bin/env bash

# This is a WIP script to attempt compilation of DICOMautomaton into Web Assembly via the Emscripten SDK.

set -eu

create_sh=$(mktemp '/tmp/dcma_compile_webassembly_internal_XXXXXXXXX.sh')

trap "{ rm '${create_sh}' ; }" EXIT # Clean-up the temp file when we exit.

printf \
'#!/bin/bash

set -eu

# option 1.
pacman -Sy --noconfirm emscripten
source /etc/profile.d/emscripten.sh

## option 2.
## Install the latest emsdk.
#mkdir /emsdk/
#cd /emsdk/
#git clone "https://github.com/emscripten-core/emsdk.git"
#cd emsdk/
#./emsdk install latest
#./emsdk activate latest
#source ./emsdk_env.sh
#emcc -v # Runs basic sanity checks.


# Gather knowledge about the "Emscripten ports" before proceeding.
#emcc --show-ports


# Install a few packages manually to /wip_all/ for later linking and header inclusion.
# Note that system packages are intentionally hidden from the emsdk.

# ... TODO ...


rm -rf /ygorcluster/build || true
cd /ygorcluster/
mkdir build
cd build
emcmake \
  cmake \
    ../ \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release 
emmake make -j 8 VERBOSE=1 WASM=1
#emmake install DESTDIR=/scratch/ygorcluster
emmake make install DESTDIR=/wip_all/


rm -rf /explicator/build || true
cd /explicator/
mkdir build
cd build/
emcmake \
  cmake \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release \
    ..
emmake make VERBOSE=1 WASM=1
#emmake make install DESTDIR=/scratch/explicator
emmake make install DESTDIR=/wip_all/


rm -rf /ygor/build || true
cd /ygor/
mkdir build
cd build
emcmake \
  cmake -E env CXXFLAGS="-s USE_BOOST_HEADERS=1" \
  cmake \
    ../ \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release \
    -DWITH_LINUX_SYS=OFF \
    -DWITH_EIGEN=OFF \
    -DWITH_GNU_GSL=OFF \
    -DWITH_BOOST=ON
emmake \
  make \
    -j 8 \
    VERBOSE=1 \
    WASM=1 
emmake make install DESTDIR=/wip_all/


rm -rf /asio/build || true
mkdir /asio/
cd /asio/
wget "https://sourceforge.net/projects/asio/files/asio/1.14.0%20%28Stable%29/asio-1.14.0.tar.bz2"
tar -axf asio-1.14.0.tar.bz2
cd asio-1.14.0/
emconfigure \
  ./configure \
    --prefix=/usr
emmake make -j 8 VERBOSE=1
emmake make install DESTDIR=/wip_all/


#### Stuck on getting Boost to honour emscripten.   :(

rm -rf /boost/ || true
mkdir /boost/
cd /boost/
wget "https://dl.bintray.com/boostorg/release/1.73.0/source/boost_1_73_0.tar.bz2"
tar -axf boost_1_73_0.tar.bz2
cd boost_1_73_0/
#   toolset=gcc \
#    --with-toolset=gcc \
#export LDFLAGS="-s WASM=1"
emconfigure \
  ./bootstrap.sh \
    --with-toolset=gcc \
    --prefix=/wip_all/usr \
    --without-icu \
    --with-libraries="filesystem,serialization,iostreams,thread,system"
emconfigure \
  ./b2 \
    toolset=gcc \
    link=static \
    variant=release \
    threading=multi \
    --prefix=/wip_all/usr \
    --build-type=minimal \
    install

...
sed -i -e "g/gmp/d" -e "g/mpfr/g" src/CMakeLists.txt
...

rm -rf /dcma/build || true
cd /dcma/
mkdir build
cd build
#    -DCMAKE_PREFIX_PATH=/wip_all/usr/include \
#    -DCMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES=/wip_all/usr/include \
#    CXXFLAGS="-s USE_BOOST_HEADERS=1 -s USE_ZLIB=1 -I/wip_all/usr/include" \
#    LDFLAGS="-s USE_BOOST_HEADERS=1 -s USE_ZLIB=1 -L/wip_all/usr/lib" \
emcmake \
  cmake -E env \
    CXXFLAGS="-s USE_ZLIB=1 -I/wip_all/usr/include" \
    LDFLAGS="-s USE_ZLIB=1 -L/wip_all/usr/lib" \
  cmake \
    -DMEMORY_CONSTRAINED_BUILD=OFF \
    -DWITH_ASAN=OFF \
    -DWITH_TSAN=OFF \
    -DWITH_MSAN=OFF \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_BUILD_TYPE=Release \
    -DWITH_EIGEN=OFF \
    -DWITH_CGAL=OFF \
    -DWITH_NLOPT=OFF \
    -DWITH_SFML=OFF \
    -DWITH_WT=OFF \
    -DWITH_GNU_GSL=OFF \
    -DWITH_POSTGRES=OFF \
    -DWITH_JANSSON=OFF \
    ../
emmake \
  make \
    -j 8 \
    VERBOSE=1 \
    WASM=1 \
    dicomautomaton_dispatcher


' > "$create_sh"
chmod 777 "$create_sh"

sudo docker run -it --rm \
    --network=host \
    -v "$(pwd)":/scratch/:rw \
    -v "$create_sh":/start/create.sh:ro \
    -w /scratch/ \
    dicomautomaton_webserver_arch:latest \
    /start/create.sh 




exit
exit
exit

Linking errors:

  [100%] Building CXX object src/CMakeFiles/dicomautomaton_dispatcher.dir/DICOMautomaton_Dispatcher.cc.o
  cd /dcma/build/src && /wasm/emsdk/upstream/emscripten/em++  -DUSTREAM_H @CMakeFiles/dicomautomaton_dispatcher.dir/includes_CXX.rsp -s USE_BOOST_HEADERS=1 -s USE_ZLIB=1 -I/wip_all/usr/include -fdiagnostics-color=always -Wno-deprecated -Wall -Wextra -Wpedantic -O3 -DNDEBUG   -UDCMA_USE_EIGEN -UDCMA_USE_CGAL -UDCMA_USE_NLOPT -UDCMA_USE_SFML -UDCMA_USE_WT -UDCMA_USE_POSTGRES -UDCMA_USE_JANSSON -UDCMA_USE_GNU_GSL -w -std=c++17 -o CMakeFiles/dicomautomaton_dispatcher.dir/DICOMautomaton_Dispatcher.cc.o -c /dcma/src/DICOMautomaton_Dispatcher.cc
  [100%] Linking CXX executable ../bin/dicomautomaton_dispatcher.js
  cd /dcma/build/src && /usr/bin/cmake -E cmake_link_script CMakeFiles/dicomautomaton_dispatcher.dir/link.txt --verbose=1
  /wasm/emsdk/upstream/emscripten/em++  -s USE_BOOST_HEADERS=1 -s USE_ZLIB=1 -I/wip_all/usr/include -fdiagnostics-color=always -Wno-deprecated -Wall -Wextra -Wpedantic -O3 -DNDEBUG  -s USE_BOOST_HEADERS=1 -s USE_ZLIB=1 -L/wip_all/usr/lib -O2 @CMakeFiles/dicomautomaton_dispatcher.dir/objects1.rsp  -o ../bin/dicomautomaton_dispatcher.js @CMakeFiles/dicomautomaton_dispatcher.dir/linklibs.rsp
  wasm-ld: error: unknown file type: operations.o
  em++: error: '/wasm/emsdk/upstream/bin/wasm-ld @/tmp/emscripten__dcjags3.rsp' failed (1)
  make[3]: *** [src/CMakeFiles/dicomautomaton_dispatcher.dir/build.make:540: bin/dicomautomaton_dispatcher.js] Error 1
  make[3]: Leaving directory '/dcma/build'
  make[2]: *** [CMakeFiles/Makefile2:892: src/CMakeFiles/dicomautomaton_dispatcher.dir/all] Error 2
  make[2]: Leaving directory '/dcma/build'
  make[1]: *** [CMakeFiles/Makefile2:899: src/CMakeFiles/dicomautomaton_dispatcher.dir/rule] Error 2
  make[1]: Leaving directory '/dcma/build'
  make: *** [Makefile:505: dicomautomaton_dispatcher] Error 2


Duplicate function signatures in Explicator and Ygor (fixed):

  [100%] Linking CXX executable ../bin/dicomautomaton_dispatcher.js
  cd /dcma/build/src && /usr/bin/cmake -E cmake_link_script CMakeFiles/dicomautomaton_dispatcher.dir/link.txt --verbose=1
  /wasm/emsdk/upstream/emscripten/em++  -s USE_BOOST_HEADERS=1 -s USE_ZLIB=1 -I/wip_all/usr/include -fdiagnostics-color=always -Wno-deprecated -Wall -Wextra -Wpedantic -O3 -DNDEBUG  -s USE_BOOST_HEADERS=1 -s USE_ZLIB=1 -L/wip_all/usr/lib -O2 @CMakeFiles/dicomautomaton_dispatcher.dir/objects1.rsp  -o ../bin/dicomautomaton_dispatcher.js @CMakeFiles/dicomautomaton_dispatcher.dir/linklibs.rsp
  wasm-ld: error: duplicate symbol: Does_File_Exist_And_Can_Be_Read(std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> > const&)
  >>> defined in /wip_all/usr/lib/libexplicator.a(Files.cc.o)
  >>> defined in /wip_all/usr/lib/libygor.a(YgorFilesDirs.cc.o)
  
  wasm-ld: error: duplicate symbol: Does_Dir_Exist_And_Can_Be_Read(std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> > const&)
  >>> defined in /wip_all/usr/lib/libexplicator.a(Files.cc.o)
  >>> defined in /wip_all/usr/lib/libygor.a(YgorFilesDirs.cc.o)
  
  wasm-ld: error: duplicate symbol: NGrams_With_Occurence(std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> > const&, long, long, unsigned char const&)
  >>> defined in /wip_all/usr/lib/libexplicator.a(String.cc.o)
  >>> defined in /wip_all/usr/lib/libygor.a(YgorString.cc.o)
  
  wasm-ld: error: duplicate symbol: NGrams(std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> > const&, long, long, unsigned char const&)
  >>> defined in /wip_all/usr/lib/libexplicator.a(String.cc.o)
  >>> defined in /wip_all/usr/lib/libygor.a(YgorString.cc.o)
  
  wasm-ld: error: duplicate symbol: NGram_Matches(std::__2::set<std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> >, std::__2::less<std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> > >, std::__2::allocator<std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> > > > const&, std::__2::set<std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> >, std::__2::less<std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> > >, std::__2::allocator<std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> > > > const&)
  >>> defined in /wip_all/usr/lib/libexplicator.a(String.cc.o)
  >>> defined in /wip_all/usr/lib/libygor.a(YgorString.cc.o)
  
  wasm-ld: error: duplicate symbol: NGram_Match_Count(std::__2::set<std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> >, std::__2::less<std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> > >, std::__2::allocator<std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> > > > const&, std::__2::set<std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> >, std::__2::less<std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> > >, std::__2::allocator<std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> > > > const&)
  >>> defined in /wip_all/usr/lib/libexplicator.a(String.cc.o)
  >>> defined in /wip_all/usr/lib/libygor.a(YgorString.cc.o)
  
  wasm-ld: error: duplicate symbol: ALongestCommonSubstring(std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> > const&, std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> > const&)
  >>> defined in /wip_all/usr/lib/libexplicator.a(String.cc.o)
  >>> defined in /wip_all/usr/lib/libygor.a(YgorString.cc.o)
  
  wasm-ld: error: duplicate symbol: Canonicalize_String2(std::__2::basic_string<char, std::__2::char_traits<char>, std::__2::allocator<char> > const&, unsigned char const&)
  >>> defined in /wip_all/usr/lib/libexplicator.a(String.cc.o)
  >>> defined in /wip_all/usr/lib/libygor.a(YgorString.cc.o)
  
  wasm-ld: error: unknown file type: operations.o
  em++: error: '/wasm/emsdk/upstream/bin/wasm-ld @/tmp/emscripten_vs4jcjmj.rsp' failed (1)
  make[3]: *** [src/CMakeFiles/dicomautomaton_dispatcher.dir/build.make:540: bin/dicomautomaton_dispatcher.js] Error 1
  make[3]: Leaving directory '/dcma/build'
  make[2]: *** [CMakeFiles/Makefile2:892: src/CMakeFiles/dicomautomaton_dispatcher.dir/all] Error 2
  make[2]: Leaving directory '/dcma/build'
  make[1]: *** [CMakeFiles/Makefile2:899: src/CMakeFiles/dicomautomaton_dispatcher.dir/rule] Error 2
  make[1]: Leaving directory '/dcma/build'
  make: *** [Makefile:505: dicomautomaton_dispatcher] Error 2
  
