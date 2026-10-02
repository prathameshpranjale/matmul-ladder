# Results Log

Everything we measured or tested, in order, including runs that turned out misleading. Numbers are copied from real runs. Newest entries go at the bottom of each project section.

Machine: Intel Core i5-1235U (12 logical CPUs, AVX2 and FMA, no AVX-512), 7.7 GB RAM, Windows 11 + WSL2 Ubuntu 24.04, gcc 13.3. No NVIDIA GPU.

---

# Project A: edu-pci-driver (Linux PCI driver in QEMU)

Repo: https://github.com/prathameshpranjale/edu-pci-driver

| Milestone | What was tested | Result |
|---|---|---|
| M0 environment | Kernel 6.6.87 built, boots in QEMU with KVM | Boots in about 2 s |
| M1 to M3 | probe, ID register, liveness check | `edu ID register = 0x010000ed (version 1.0)`, `liveness check passed` |
| M4 interrupt, MSI attempt | MSI with KVM | Device computed 3628800 and showed irq pending, handler never ran (timeout) |
| M4 interrupt, INTx test | Plain INTx | `10! = 3628800 via irq 10 (OK)` |
| M4 interrupt, MSI without KVM | Same code, software emulation | Still no interrupt: the `edu` model does not deliver MSI (QEMU 8.2.2) |
| M5 char device | `test_edu`: 0! to 12! via ioctl | PASS, 0 failures |
| M6 concurrency | `test_stress`: 8 processes x 500 requests, 4 CPUs | PASS, 4000 of 4000 correct |
| M6 mutation | Mutex removed in a copy | Stress test FAILED: 8 of 8 workers got wrong results (single-caller test still passed) |
| M7 DMA | `test_dma`: 7 sizes (1 to 4096) + 2 invalid lengths | PASS, 0 failures, first try |
| M7 mutation | Return DMA skipped in a copy | `test_dma` FAILED on all 7 sizes |
| M8 KASAN + lockdep kernel | All 3 tests on debug kernel | PASS, `KERNEL-CLEAN` |
| M8 mutation | 1-byte heap overflow planted in a copy | All functional tests passed, KASAN reported `slab-out-of-bounds in edu_ioctl`, gate failed the run |
| Not tested | lockdep never seen to fire | Open gap |

### CI history (GitHub Actions)

| Commit | Result | Why |
|---|---|---|
| 0491229 | FAILED at "Build kernel", exit 126 | Scripts committed as 100644 (not executable) |
| 2758576 | Build kernel OK (about 9 min), tests passed, job FAILED | Health check matched harmless `RETBleed: WARNING ... vulnerable` boot notice |
| b291c61 | SUCCESS, about 10 min total | Health check narrowed to real warnings |

---

# Project B: matmul-ladder (matrix multiply optimization, CPU first)

Metric: GFLOPS = 2 * n^3 / seconds. Higher is better. "noise" = (slowest - fastest) / median over the runs.
Compiler flags: `-O2 -fno-tree-vectorize` (no auto-SIMD, so each step shows only its own change).

### B1. Environment check
Missing: `perf`, OpenBLAS, NVIDIA GPU / `nvidia-smi` / `nvcc` / cuBLAS. Windows shows only Intel UHD Graphics. Decision: CPU steps now; GPU steps need another machine or a cloud GPU.

### B2. Steps 1 and 2, first run (7 runs for small n, 3 for large, not pinned)

| n | v1 naive | v2 reorder |
|---|---|---|
| 256 | 1.980 | 2.964 |
| 512 | 1.552 | 3.250 |
| 1024 | 0.399 | 2.838 |

### B3. Same code, repeated (revealed noise)

| n | v1 naive | v2 reorder |
|---|---|---|
| 512 | 1.188 | **0.948** (was 3.250 in B2) |
| 1024 | 0.400 | 3.254 |

Lesson: one run is not a result. Fix applied: more runs, pin to one core, noise column.

### B4. After the harness fix (pinned, 15/11/5 runs), two back-to-back runs

| n | v1 A / B | v2 A / B | noise range |
|---|---|---|---|
| 256 | 2.121 / 1.849 | 3.464 / 3.150 | 18% to 28% |
| 512 | 1.249 / 1.385 | 3.446 / 3.358 | 11% to 15% |
| 1024 | 0.426 / 0.422 | 3.230 / 3.360 | 3% to 9% |

### B5. Correctness gate check
v2 with the last column skipped: harness printed `WRONG RESULT` (max error 0.63) and timed nothing. Gate works.

### B6. Step 3 first tile sweep, one pass each, in sequence (MISLEADING, see B8)

Baseline v2 in the same pass: 512: 3.431, 1024: 3.392.

| BLOCK | n=512 | n=1024 |
|---|---|---|
| 16 | 2.820 | 2.315 |
| 32 | 1.922 | 1.872 |
| 48 | 1.844 | 1.861 |
| 64 | 2.201 | 2.117 |
| 96 | 2.431 | 2.419 |
| 128 | 2.583 | 2.518 |
| 256 | 2.485 | 2.402 |

Appeared to show blocking is slower than v2. n=2048: v2 2.653, BLOCK=64 2.333, BLOCK=128 2.919.

### B7. Full run with BLOCK=128 (contradicts B6)

| n | v1 | v2 | v3 |
|---|---|---|---|
| 256 | 1.841 | 2.568 | 3.408 |
| 512 | 1.352 | 2.734 | 3.379 |
| 1024 | 0.419 | 2.566 | 3.067 |

Edge sizes (not multiples of the tile): n=100, 130, 257, 300 all correct.

### B8. Alternating comparison, BLOCK=128, n=1024, 6 rounds
v2 median 2.756 (range 2.669 to 2.813); v3 median 3.070 (range 3.034 to 3.121). Ranges do not overlap: blocking is faster. B6 was a measurement artifact of the machine drifting.

### B9. Rotating tile sweep, n=1024, median of 3 rounds

| BLOCK | 16 | 32 | 64 | 96 | 128 | 256 |
|---|---|---|---|---|---|---|
| GFLOPS | 2.272 | 2.501 | 2.350 | 2.593 | 3.089 | 3.217 |

n=2048, BLOCK 128 vs 256: 3.007 vs 3.183 (non-overlapping rounds). Default set to 256.

### B10. Final step 2 vs step 3 (BLOCK=256), alternating

| n | v2 median (range) | v3 median (range) | Rounds | Firmness |
|---|---|---|---|---|
| 1024 | 2.741 (2.582 to 2.835) | 3.141 (3.054 to 3.217) | 6 | Clear, about +15% |
| 2048 | 2.895 (2.545 to 3.088) | 3.696 (2.947 to 3.747) | 3 | Ranges overlap, likely not firm |

### B11. Full run, BLOCK=256 (a fast-machine session: all numbers higher than before)

| n | v1 | v2 | v3 |
|---|---|---|---|
| 256 | 2.379 | 3.127 | 4.211 |
| 512 | 1.509 | 3.463 | 4.162 |
| 1024 | 0.591 | 3.667 | 4.230 |

Same code gave v1 = 0.42 earlier and 0.59 here: sessions differ by 30% to 40%. Quote ratios from alternating comparisons, not absolute numbers from different days.

### B12. Step 4 (SIMD, AVX2 + FMA) and step 5 (threads) first correctness check
Sizes 100, 130, 257, 300, 512 (odd sizes on purpose): all 5 versions pass. FMA changes rounding slightly (max error 2.9e-07 vs 2.9e-07 .. 4.9e-07), well inside the 1e-4 tolerance. Tiny sizes (n=100 to 300) are meaningless for threads (noise up to 3387%: thread start-up dominates).

### B13. Step 3 vs step 4, alternating, n=1024, 6 rounds

| Version | Median | Range (GFLOPS) |
|---|---|---|
| v3_blocked | 4.097 | 3.929 .. 4.531 |
| v4_simd | 27.363 | 25.712 .. 28.579 |

Ranges far apart: SIMD is about 6.7x faster than blocked scalar.

### B14. Thread scaling, first look (PBLOCK=64), n=1024, 3 rounds, rotating thread counts

| Version | Median | Range |
|---|---|---|
| v4_simd (1 thread) | 25.672 | 22.884 .. 26.859 |
| v5_threads 1 thr | 19.755 | 18.212 .. 20.501 |
| 2 thr | 33.039 | 26.979 .. 43.169 |
| 4 thr | 42.677 | 40.474 .. 46.521 |
| 6 thr | 55.288 | 54.653 .. 55.892 |
| 8 thr | 63.374 | 61.527 .. 65.713 |
| 12 thr | 71.252 | 69.292 .. 72.761 |

Odd: step 5 with 1 thread (19.8) was slower than step 4 (25.7) although it is the same kernel.

### B15. PBLOCK sweep (thread-tile size), n=1024, 3 rounds, median

| PBLOCK | 12 threads | 1 thread |
|---|---|---|
| 32 | 51.2 | 14.6 |
| 64 | 61.0 | 20.3 |
| 128 | 69.6 | 24.4 |
| 256 | 67.2 | 27.0 |

Default set to PBLOCK=128 (256 matches step 4 on one thread but gives only 16 tiles at n=1024, too few to balance 12 threads). Note the 12-thread value for PBLOCK=64 was 71.3 in B14 and 61.0 here: sessions differ again.

### B16. FINAL RUN (scripts/final_run.sh, raw output in matmul-ladder/bench/history/20261002-174143-final.txt)
BLOCK=256, PBLOCK=128, 12 threads for step 5, single-thread versions pinned to CPU 2. Single pass per size; the noise column is the spread across runs.

| n | v1 naive | v2 reorder | v3 blocked | v4 SIMD | v5 threads (12) |
|---|---|---|---|---|---|
| 256 | 2.719 | 3.406 | 4.338 | 35.766 | 19.621 (noise 412%, not meaningful) |
| 512 | 2.211 | 3.420 | 4.216 | 27.923 | 69.548 |
| 1024 | 0.613 | 3.535 | 4.310 | 27.936 | 82.808 |
| 2048 | not run (too slow) | 3.145 | 4.080 | 21.337 | 80.690 |

All correct (max error about 1e-6). Total at n=1024: naive 0.613 to threads 82.8 = about 135x.

### B17. Thread scaling, final (3 rounds, rotating, medians with range)

| Threads | n=1024 | n=2048 |
|---|---|---|
| v4_simd (1 thread, reference) | 26.997 (26.355 .. 27.672) | 26.571 (20.952 .. 26.939) |
| v5, 1 | 23.897 (23.615 .. 24.017) | 22.871 (20.623 .. 23.070) |
| 2 | 48.877 (47.498 .. 49.883) | 39.914 (37.355 .. 41.333) |
| 4 | **47.609** (45.772 .. 51.599) | 55.284 (55.244 .. 61.862) |
| 6 | 59.550 (59.315 .. 63.472) | 69.730 (63.548 .. 72.543) |
| 8 | 70.759 (69.749 .. 74.838) | 77.956 (74.264 .. 86.355) |
| 12 | 81.684 (79.429 .. 89.483) | 90.319 (89.864 .. 96.602) |

12 threads vs step 4: 3.0x at n=1024, 3.4x at n=2048. At n=1024, going from 2 to 4 threads gave no gain (48.9 vs 47.6).

### B18. Presentation page checks (`matmul-ladder/ui/index.html`, headless Edge)
- All 4 charts, 64 thread tiles, 128 memory cells and 12 challenge cards render; no `undefined` values in the page.
- Memory animation, 16 reads per panel, clicked through programmatically:

| Cache size | Naive (column walk) | Reordered (row walk) |
|---|---|---|
| 4 lines | 16 misses | 4 misses |
| 8 lines | 8 misses | 4 misses |
| 16 lines | 8 misses | 4 misses |

Matches the hand-computed expectation (with 4 lines the column's 8 distinct lines thrash; with 8 or more, the second column re-hits). This is an illustration of the idea, not a measurement of the real CPU.
- Visual fixes after screenshots: the threads chart's reference-line label covered the bars (moved to the legend line); the end-of-run summary only appeared on an extra click (now shows on the last read).

---

*(Anything measured after this point is appended below.)*
