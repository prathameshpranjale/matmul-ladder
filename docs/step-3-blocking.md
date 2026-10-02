# Step 3: cache blocking (tiling)

Plain-words notes. Machine: Intel Core i5-1235U, WSL2, gcc 13.3, `-O2 -fno-tree-vectorize`, pinned to one core.

## The idea

Step 2 reads a whole row of `B` for every `(i, k)`. For big matrices that is a lot of data, and `B` no longer fits in the cache closest to the core.

Step 3 cuts the three loops into `BLOCK`-sized pieces. Inside one tile the code touches only a `BLOCK x BLOCK` piece of `B` and `BLOCK`-wide slices of `A` and `C`, so that data can stay in cache while it is reused.

```
for ii (step BLOCK): for kk (step BLOCK): for jj (step BLOCK):
    for i in tile: for k in tile: a = A[i][k]; for j in tile: C[i][j] += a * B[k][j]
```

The tile loops run in order, so each `C` element still adds its `k` terms in the same order as step 2. The results are identical to step 2 (same error values in the output).

## Results

Measured by alternating the two versions across several rounds (`scripts/compare_ab.sh`), because the machine's speed drifts.

| n | Step 2 reorder (median, range) | Step 3 blocked, tile 256 (median, range) | Rounds | How firm |
|---|---|---|---|---|
| 1024 | 2.74 (2.58 to 2.84) GFLOPS | 3.14 (3.05 to 3.22) GFLOPS | 6 | Clear: ranges do not overlap, about +15% |
| 2048 | 2.90 (2.55 to 3.09) GFLOPS | 3.70 (2.95 to 3.75) GFLOPS | 3 | Likely, not firm: ranges overlap, only 3 rounds |

Tile size sweep at n=1024 (`scripts/sweep_block.sh`, median of 3 rounds, rotating through sizes):

| BLOCK | 16 | 32 | 64 | 96 | 128 | 256 |
|---|---|---|---|---|---|---|
| GFLOPS | 2.27 | 2.50 | 2.35 | 2.59 | 3.09 | 3.22 |

At n=2048, tile 256 (3.18) beat tile 128 (3.01) with non-overlapping rounds. Default is `BLOCK=256`.

## What the numbers show

- Blocking helps, but **modestly** (roughly 15% at n=1024), not by multiples. It is far smaller than the 8x from step 2.
- **Small tiles are slower than no blocking at all** (tile 16 to 96 are below step 2). Likely reasons: more loop overhead and short inner loops. That is a guess; we did not measure it.
- Bigger tiles won: a 256 x 256 float tile of `B` is 256 KB. That is probably small enough for a performance core's L2 cache (about 1.25 MB for this CPU generation: an estimate from the architecture and the `lscpu` totals, **not verified per core**), but too big for L1 (about 48 KB). A hypothesis that fits the data, nothing more.
- Why the gain is modest: the scalar inner loop does two loads and a store per multiply-add. Blocking keeps data in cache, but it does not reduce those loads and stores. The loop is probably limited by that, not by memory. **This is a hypothesis; we have no counter data** (`perf` is not available here). Steps 4 and onward (SIMD and register tiling) attack the instruction cost directly.

## How we measured, and a lesson about drifting machines

- **A first sweep gave the wrong answer.** It ran each tile size once, one after another. It suggested blocking was *slower* than step 2 and chose tile 128. A full run later showed the opposite. The machine's speed varies by 25% to 40% from minute to minute (and between sessions), so single runs in sequence are unreliable.
- **Fix:** alternate versions across rounds, report medians and ranges, and call a difference real only when the ranges do not overlap.
- **Absolute numbers are not stable across sessions.** For the same code, naive n=1024 measured 0.40 to 0.59 GFLOPS and reorder 2.6 to 3.7 in different sessions. Quote **ratios from one alternating comparison**, not absolute numbers from different days.
- **Edge sizes work.** n=100, 130, 257 and 300 (not multiples of the tile) all pass the correctness check.

## Honest limits

- No hardware counters, so every "why" above is a hypothesis that fits the data.
- The 2048 result has only 3 rounds.
- One machine, one core.

## Next

Step 4: SIMD (AVX2). This CPU has AVX2 and FMA, so each instruction can process 8 floats. That attacks the instruction-throughput limit directly.
