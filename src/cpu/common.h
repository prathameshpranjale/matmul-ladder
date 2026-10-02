/* Shared declarations for the CPU ladder.
 * All matrices are n x n, row-major, float. C = A * B.
 * Every version OVERWRITES C (it must not assume C is zeroed). */
#ifndef MATMUL_COMMON_H
#define MATMUL_COMMON_H

/* Step 0: trusted answer. Accumulates in double, writes a double result. */
void matmul_reference(const float *A, const float *B, double *Cref, int n);

/* Step 1: naive triple loop, order i, j, k. */
void matmul_v1_naive(const float *A, const float *B, float *C, int n);

/* Step 2: same math, loop order i, k, j (walks B and C along rows). */
void matmul_v2_reorder(const float *A, const float *B, float *C, int n);

/* Step 3: cache blocking. BLOCK is the tile edge in elements (set with -DBLOCK=..., see Makefile). */
#ifndef BLOCK
#define BLOCK 256	/* best of 16..256 on the i5-1235U; see docs/step-3-blocking.md */
#endif
void matmul_v3_blocked(const float *A, const float *B, float *C, int n);

/* Step 4: same tiling as step 3, but the inner loop uses AVX2 + FMA (8 floats per instruction).
 * matmul_tile_avx2 does one (i, k, j) tile and is shared with step 5. Needs an AVX2/FMA CPU. */
void matmul_tile_avx2(const float *A, const float *B, float *C, int n,
		      int i0, int i1, int k0, int k1, int j0, int j1);
void matmul_v4_simd(const float *A, const float *B, float *C, int n);

/* Step 5: step 4's kernel with the output tiles spread over threads (OpenMP).
 * PBLOCK is the edge of the output tile one thread works on at a time. */
#ifndef PBLOCK
#define PBLOCK 128	/* see scripts/sweep_pblock.sh */
#endif
void matmul_v5_threads(const float *A, const float *B, float *C, int n);

#endif
