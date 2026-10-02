# CPU ladder. Run inside Linux/WSL.
#
# -O2 -fno-tree-vectorize: the compiler is told NOT to auto-vectorize, so each step in the ladder
# shows only the effect of its own change. (SIMD is added deliberately in step 4, via intrinsics.)
# No -march=native and no -ffast-math, for the same reason and so results stay reproducible.
# -fopenmp is global but only step 5 contains OpenMP pragmas.
#
# BLOCK  = tile edge for steps 3 and 4 (see scripts/sweep_block.sh)
# PBLOCK = output tile edge one thread works on in step 5
# BIN    = output binary

CC     ?= gcc
BLOCK  ?= 256
PBLOCK ?= 128
BIN    ?= bin/bench
CFLAGS  = -O2 -fno-tree-vectorize -fopenmp -std=c11 -Wall -Wextra -DBLOCK=$(BLOCK) -DPBLOCK=$(PBLOCK)
SRC     = src/cpu/bench.c src/cpu/v0_reference.c src/cpu/v1_naive.c src/cpu/v2_reorder.c \
          src/cpu/v3_blocked.c src/cpu/v4_simd.c src/cpu/v5_threads.c

$(BIN): $(SRC) src/cpu/common.h Makefile
	mkdir -p $(dir $(BIN))
	$(CC) $(CFLAGS) -DCFLAGS_STR='"$(CFLAGS)"' -o $@ $(SRC) -lm

clean:
	rm -rf bin

.PHONY: clean
