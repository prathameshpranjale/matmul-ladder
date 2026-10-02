/* Step 5: multithreading (OpenMP) on top of step 4.
 *
 * C is cut into PBLOCK x PBLOCK output tiles. Each tile is computed completely by ONE thread
 * (it zeroes its own piece of C, then walks all the k blocks), so no two threads ever write the
 * same C element: no locks, no atomics, no races. Threads read A and B, which never change.
 *
 * Thread count comes from OMP_NUM_THREADS (default: all CPUs the OS reports).
 * Why not split by rows only? With few rows per thread, the work is uneven; many small tiles
 * keep every thread busy (schedule(static) is enough because all tiles cost the same). */
#include <string.h>

#include "common.h"

static inline int min_int(int a, int b)
{
	return a < b ? a : b;
}

void matmul_v5_threads(const float *A, const float *B, float *C, int n)
{
#pragma omp parallel for collapse(2) schedule(static)
	for (int ii = 0; ii < n; ii += PBLOCK) {
		for (int jj = 0; jj < n; jj += PBLOCK) {
			int i1 = min_int(ii + PBLOCK, n);
			int j1 = min_int(jj + PBLOCK, n);

			for (int i = ii; i < i1; i++)
				memset(&C[i * n + jj], 0, sizeof(float) * (size_t)(j1 - jj));

			for (int kk = 0; kk < n; kk += BLOCK)
				matmul_tile_avx2(A, B, C, n, ii, i1, kk, min_int(kk + BLOCK, n), jj, j1);
		}
	}
}
