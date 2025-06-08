1 - Makefile content:

CC=/usr/cfa_rainbow_breaker_fourier/gcc-arm-8.3-2019.02-x86_64-arm-linux-gnueabi/bin/arm-linux-gnueabi-gcc \
CXX=/usr/cfa_rainbow_breaker_fourier/gcc-arm-8.3-2019.02-x86_64-arm-linux-gnueabi/bin/arm-linux-gnueabi-g++ \
F77=/usr/cfa_rainbow_breaker_fourier/gcc-arm-8.3-2019.02-x86_64-arm-linux-gnueabi/bin/arm-linux-gnueabi-gfortran \
./configure --host=arm-linux-gnueabi \
            --prefix=$(pwd)/build_arm \
            --enable-static \
            --disable-shared \
            --enable-float \
            --enable-openmp \
            --enable-neon \
            CFLAGS="-O3 -Wall -std=c11 -fPIC -mfpu=neon-vfpv4 -mfloat-abi=softfp -march=armv7-a -fstrict-aliasing -ffast-math" \
            CXXFLAGS="-O3 -Wall -std=c++11 -fPIC -mfpu=neon-vfpv4 -mfloat-abi=softfp -march=armv7-a -fstrict-aliasing -ffast-math" \
            F77FLAGS="-O3 -Wall -fPIC -mfpu=neon-vfpv4 -mfloat-abi=softfp -march=armv7-a -fstrict-aliasing -ffast-math"


2 - Commands to build:
make
make install