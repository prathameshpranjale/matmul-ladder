/* Step 4: SIMD (AVX2 + FMA).
 *
 * Same tiling as step 3. The inner loop used to do one multiply-add per iteration (two loads, one
 * multiply-add, one store). Now each iteration handles 8 floats in one instruction:
 *     c[0..7] += a * b[0..7]      (a is broadcast to all 8 lanes)
 * so the instruction count per useful operation drops by about 8x.
 *
 * Only the functions below are compiled for AVX2/FMA (target attribute); the rest of the program
 * stays plain scalar code, and bench checks the CPU supports it before calling this.
 * FMA rounds once instead of twice, so results differ from the scalar versions in the last bits
 * (well inside the benchmark tolerance). */
#include <immintrin.h>
#include <string.h>

#include "common.h"

/* One (i, k, j) tile: for i in [i0,i1), k in [k0,k1), j in [j0,j1):  C[i][j] += A[i][k] * B[k][j] */
__attribute__((target("avx2,fma")))
void matmul_tile_avx2(const float *A, const float *B, float *C, int n,
		      int i0, int i1, int k0, int k1, int j0, int j1)
{
	for (int i = i0; i < i1; i++) {
		for (int k = k0; k < k1; k++) {
			float a = A[i * n + k];
			__m256 va = _mm256_set1_ps(a);
			int j = j0;

			for (; j + 8 <= j1; j += 8) {
				__m256 c = _mm256_loadu_ps(&C[i * n + j]);
				__m256 b = _mm256_loadu_ps(&B[k * n + j]);

				c = _mm256_fmadd_ps(va, b, c);
				_mm256_storeu_ps(&C[i * n + j], c);
			}
			for (; j < j1; j++)	/* leftover columns when the tile width is not a multiple of 8 */
				C[i * n + j] += a * B[k * n + j];
		}
	}
}

static inline int min_int(int a, int b)
{
	return a < b ? a : b;
}

__attribute__((target("avx2,fma")))
void matmul_v4_simd(const float *A, const float *B, float *C, int n)
{
	memset(C, 0, sizeof(float) * (size_t)n * (size_t)n);

	for (int ii = 0; ii < n; ii += BLOCK)
		for (int kk = 0; kk < n; kk += BLOCK)
			for (int jj = 0; jj < n; jj += BLOCK)
				matmul_tile_avx2(A, B, C, n,
						 ii, min_int(ii + BLOCK, n),
						 kk, min_int(kk + BLOCK, n),
						 jj, min_int(jj + BLOCK, n));
}
