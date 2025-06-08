# === Configuration ===
CC = ./gcc-arm-8.3-2019.02-x86_64-arm-linux-gnueabi/bin/arm-linux-gnueabi-gcc
CFLAGS = -O3 -march=armv7-a -fPIC -shared -Wall -mfloat-abi=softfp -fvisibility=hidden -mfpu=neon-vfpv4 -std=c11 -fopenmp -fstrict-aliasing -ffast-math

LDFLAGS = -Wl,--export-dynamic -Wl,-rpath,'$$ORIGIN'

SRC = color_detect.c
OUT = color_detect.so

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS)

clean:
	rm -f $(OUT)