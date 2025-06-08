# === Configuration ===
CC = ./gcc-arm-8.3-2019.02-x86_64-arm-linux-gnueabi/bin/arm-linux-gnueabi-gcc

CFLAGS = -O3 -march=armv7-a -fPIC -shared -Wall -mfloat-abi=softfp -mfpu=neon-vfpv4 -std=c11 -fopenmp -fstrict-aliasing -ffast-math

LDFLAGS = -Wl,--export-dynamic -Wl,--no-as-needed -Wl,-rpath,'$$ORIGIN' -L. \
          -lfftw3f_omp -lfftw3f -lm

SRC = moire_filter_fftw_eco.c
OUT = moire_filter_fftw_eco.so

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(OUT)
