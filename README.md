# matmul-ladder

Matrix multiplication, optimized one step at a time, with a measurement after every step. The point is to show **why** each step helps (memory access, vector units, threads), not just that it does.

**Status:** CPU steps 1 to 5 done and measured. GPU steps (CUDA) are not done: this machine has no NVIDIA GPU.

**Presentation:** open `ui/index.html` in a browser. It explains each step with animations and the measured results.

## Result (Intel Core i5-1235U, gcc 13.3, n=1024, final run)

| Step | What changed | GFLOPS | vs previous |
|---|---|---|---|
| 1 | Naive triple loop | 0.61 | |
| 2 | Reorder loops (i, k, j) | 3.54 | 5.8x |
| 3 | Cache blocking (tile 256) | 4.31 | 1.2x |
| 4 | SIMD (AVX2 + FMA) | 27.94 | 6.5x |
| 5 | 12 threads (OpenMP) | 82.81 | 3.0x |

About **135x** from step 1 to step 5. Every version is checked against a double-precision reference before it is timed.

How firm: single pass per size in the final run (noise 3% to 26%); close comparisons (blocking, SIMD) were confirmed by alternating runs. This laptop's speed drifts by 30% to 40% between sessions, so quote the ratios, not the absolute numbers. No hardware counters (`perf` does not work here), so the explanations in the notes are hypotheses that fit the data.

## Credits (fill in as sources are used)

- Technique write-ups used for each step: _to be listed_
- Intended reference libraries (not yet compared): OpenBLAS (CPU), cuBLAS (GPU)

## The ladder

| Step | Change | Where it runs | Status |
|---|---|---|---|
| 0 | Reference (double precision, the right answer) | CPU | done |
| 1 | Naive triple loop | CPU | done |
| 2 | Reorder loops | CPU | done |
| 3 | Cache blocking | CPU | done |
| 4 | SIMD (AVX2 + FMA) | CPU | done |
| 5 | Multithreading (OpenMP) | CPU | done |
| 6 | Naive CUDA kernel | GPU | not started: needs an NVIDIA GPU |
| 7 | Shared-memory tiled CUDA kernel | GPU | not started |
| 8 | Compare with OpenBLAS and cuBLAS | both | not started (OpenBLAS not installed) |

Possible next step on the CPU: register tiling (keep a small block of C in vector registers). Not built or measured.

## Notes per step (plain words)

- [Steps 0 to 2](docs/steps-0-to-2.md): reference, naive, reordered loops
- [Step 3](docs/step-3-blocking.md): cache blocking
- [Steps 4 and 5](docs/steps-4-and-5.md): SIMD and threads

All results and problems we hit: [docs/RESULTS_LOG.md](docs/RESULTS_LOG.md) and [docs/CHALLENGES_AND_FIXES.md](docs/CHALLENGES_AND_FIXES.md) (these two also cover a separate PCI-driver project). Raw benchmark output: `bench/history/`.

## Layout

```
matmul-ladder/
  src/cpu/      v0_reference.c  v1_naive.c  v2_reorder.c  v3_blocked.c  v4_simd.c  v5_threads.c
                bench.c (harness)  common.h
  bench/        results.json  results_2048.json  history/ (raw output of each final run)
  ui/           index.html  (presentation, no server needed)
  scripts/      see below
  docs/         notes per step
  Makefile      BLOCK (step 3/4 tile), PBLOCK (step 5 tile), BIN
```

## Run it

Needs Linux (tested on WSL2 Ubuntu 24.04) with gcc and make. No other packages for the CPU steps.

```
scripts/check_env.sh        what is installed on this machine
scripts/run_bench.sh        build, run all versions, write bench/results.json
scripts/final_run.sh        the full measurement used in this README (several minutes)
```

Measurement helpers:

```
scripts/compare_ab.sh <n> <vA> <vB> [rounds]   fair A/B comparison (alternating rounds)
scripts/sweep_block.sh                         choose BLOCK from measurements
scripts/sweep_pblock.sh                        choose PBLOCK from measurements
scripts/thread_scaling.sh [n] [rounds]         step 5 at 1, 2, 4, 6, 8, 12 threads
scripts/mutation_check.sh                      break a version on purpose; the correctness gate must catch it
```

## Measurement rules

- Each version must match the reference (relative error under 1e-4) before it is timed; the output buffer is pre-filled with NaN so a version that skips work cannot pass.
- Warm-up run, then the median of several runs; the noise column shows the spread.
- GFLOPS = 2 * n^3 / seconds.
- Single-thread versions are pinned to one core by the harness; the threaded version is not.
- Close comparisons use alternating rounds and are called real only when the ranges do not overlap.
- Compiled with `-O2 -fno-tree-vectorize` so the compiler does not add SIMD behind our back; SIMD appears only in step 4, through intrinsics.
