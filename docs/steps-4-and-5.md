# Steps 4 and 5: SIMD and threads

Plain-words notes. Machine: Intel Core i5-1235U (AVX2 + FMA, 12 logical CPUs), WSL2, gcc 13.3, `-O2 -fno-tree-vectorize -fopenmp`. All figures are in GFLOPS. Raw logs: `bench/history/`.

## Step 4: SIMD (AVX2 + FMA)

### The idea
Step 3's inner loop did one multiply-add per loop iteration (two loads, one multiply-add, one store). The CPU has 256-bit vector registers that hold **8 floats**, and an instruction that multiplies and adds all 8 at once.

```
before:  c[j]      += a * b[j]                 one float per iteration
now:     c[j..j+7] += a * b[j..j+7]            eight floats per instruction (a is copied to all 8 lanes)
```

Same tiling as step 3; only the innermost loop changed. We wrote it with intrinsics (`_mm256_fmadd_ps`) and compiled only that function for AVX2/FMA, so the earlier steps stay plain scalar code.

### Result
At n=1024 (alternating comparison, 6 rounds): step 3 median 4.10 (range 3.93 to 4.53), step 4 median 27.36 (range 25.71 to 28.58). About **6.7x**, ranges nowhere near each other. Final run: 4.31 to 27.94.

### What it shows
- Most of the earlier "slowness" was instruction count, not memory. This fits the hypothesis from step 3 (the scalar loop was limited by instructions).
- It is still far from what the CPU can do. We are doing two loads and a store for every vector multiply-add, so the loads and stores are probably the next limit. A "register tiling" micro-kernel (keep a small block of C in vector registers across the whole k loop) is the usual next step. We did **not** build or measure that. Hypothesis only.
- FMA rounds once instead of twice, so results differ from scalar in the last bits (max error stays around 1e-6).

## Step 5: multithreading (OpenMP)

### The idea
Cut `C` into 128 x 128 output tiles. Each tile is computed completely by **one** thread (it clears its own piece of `C`, then walks all `k` blocks). Because no two threads ever write the same element of `C`, there are no locks, no atomics and no races. Threads only read `A` and `B`.

### Result (3 rounds, rotating thread counts, median)

| Threads | n=1024 | n=2048 |
|---|---|---|
| step 4 (1 thread, reference) | 27.0 | 26.6 |
| 1 | 23.9 | 22.9 |
| 2 | 48.9 | 39.9 |
| 4 | 47.6 | 55.3 |
| 6 | 59.6 | 69.7 |
| 8 | 70.8 | 78.0 |
| 12 | 81.7 | 90.3 |

12 threads are about **3.0x** (n=1024) and **3.4x** (n=2048) faster than one SIMD thread. That is well short of 12x.

### What it shows, and the honest limits
- Scaling is real but **sub-linear**. At n=1024, 4 threads were no faster than 2.
- A likely reason (hypothesis, not measured): this CPU has 2 fast performance cores (4 hardware threads) and 8 slower efficiency cores; the two threads of one core share its vector units, and the OS decides where each thread runs. The 12-thread count mixes all of these.
- Very small matrices are useless for threads: start-up cost dominates (noise up to 3387% at n=100).
- One thread of step 5 is slightly slower than step 4 (23.9 vs 27.0): the output tiles are smaller. We swept the tile size (`scripts/sweep_pblock.sh`) and picked 128 as the best balance.

## The whole ladder (final run, n=1024)

| Step | GFLOPS | vs previous |
|---|---|---|
| 1 naive | 0.61 | |
| 2 reorder loops | 3.54 | 5.8x |
| 3 cache blocking | 4.31 | 1.2x |
| 4 SIMD (AVX2) | 27.94 | 6.5x |
| 5 threads (12) | 82.81 | 3.0x |

From naive to step 5 is about **135x** on this machine. Single pass per size in the final run (noise column: 3% to 26%); the close comparisons were confirmed by alternating runs. All versions match the reference.

## What we did not do
- GPU steps (CUDA): this machine has no NVIDIA GPU.
- Register tiling, or comparison against OpenBLAS (not installed). So we cannot say how close we are to a tuned library.
- Hardware counters (`perf` is not available), so every "why" is a hypothesis.
