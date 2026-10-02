# Steps 0 to 2: reference, naive, reordered loops

Plain-words notes. Numbers are from `scripts/run_bench.sh` on an Intel Core i5-1235U (WSL2, gcc 13.3, `-O2 -fno-tree-vectorize`, pinned to one core). Two back-to-back runs, shown as A / B.

## The problem

Multiply two n x n matrices: `C[i][j] = sum over k of A[i][k] * B[k][j]`. That is `2 * n^3` floating-point operations. We measure speed in GFLOPS (billions of operations per second).

## Step 0: the reference

Slow, simple, and it adds up in `double` instead of `float`. Its only job is to be **right**, so every other version has something accurate to be checked against. Nothing is timed until a version matches it (relative error under 1e-4; the real errors are around 1e-6).

## Step 1: naive loop (order i, j, k)

```c
for i: for j: for k:  sum += A[i][k] * B[k][j]
```

Memory is stored row by row. In the inner loop:
- `A[i][k]` moves along a row: the next float is the next address. Cache-friendly.
- `B[k][j]` moves **down a column**: each step jumps a whole row (n floats) ahead. Every access touches a different cache line.

## Step 2: reorder the loops (i, k, j)

```c
for i: for k: { a = A[i][k];  for j:  C[i][j] += a * B[k][j] }
```

Same arithmetic, different order. Now the inner loop walks `B` and `C` along rows, so each cache line that is loaded gets fully used, and `a` stays in a register.

## Results

| n | Step 1 naive | Step 2 reorder | Speedup |
|---|---|---|---|
| 256 | 2.12 / 1.85 GFLOPS | 3.46 / 3.15 | about 1.5 to 1.7x |
| 512 | 1.25 / 1.39 | 3.45 / 3.36 | about 2.5x |
| 1024 | 0.43 / 0.42 | 3.23 / 3.36 | about 7.6x to 8x |

Run-to-run noise was 3% to 28% depending on size (shown in the `noise` column; short runs are noisier). The n=1024 numbers are the most stable.

## What the numbers show

- The naive version gets **slower as n grows** (2.1 -> 0.4 GFLOPS), while the reordered version stays at about 3 GFLOPS. The arithmetic per element is the same, so the difference is memory access.
- The gap grows with n. That fits the explanation: for small n, matrix `B` fits in the core's cache, so the column walk is cheap; at n=1024, `B` is 4 MB, larger than the per-core cache, so the column walk keeps going out to slower memory.

**Honest limit:** that last sentence is an explanation that fits the data, not something we measured. `perf` is not available in this WSL2, so we have no cache-miss counts. Do not state cache-miss numbers.

## Gotchas hit while building this

- **Auto-vectorization would blur the ladder.** We compile with `-fno-tree-vectorize` so the compiler does not add SIMD behind our back. Each step then shows only its own change. SIMD is added on purpose in step 4.
- **Timings at small sizes were noisy** (n=512 reordered ran at 3.25 and then 0.95 GFLOPS in two runs before we changed anything). Fix: more runs for small sizes, pin the benchmark to one core, and print a `noise` column. Lesson: run it twice before believing a number.
- **A passing check must be able to fail.** `scripts/mutation_check.sh` breaks step 2 in a copy (skips the last column). The harness reports `WRONG RESULT` (error 0.63) and times nothing.
- **The result buffer is pre-filled with NaN** before the correctness run, so a version that skips work cannot pass by leaving zeros.

## What this teaches (the one-line version)

Same math, 7 to 8x faster, just by walking memory in the order it is stored.

## Next

Step 3: cache blocking (tiling). Work on small blocks of the matrices that stay in cache while they are reused.
