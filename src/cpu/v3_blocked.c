/* Step 3: cache blocking (tiling).
 *
 * Step 2 streams a whole row of B for every (i, k). For large n that is a lot of data: B no
 * longer fits in the cache closest to the core, so each pass over B comes from slower memory.
 *
 * Here the three loops are cut into BLOCK-sized pieces. Inside one (ii, kk, jj) tile the work
 * touches only a BLOCK x BLOCK piece of B and BLOCK-wide slices of A and C, small enough to stay
 * in cache while it is reused BLOCK times.
 *
 * Loop order inside is still i, k, j, and the kk blocks run in order, so for every C element the
 * k terms are added in the same order as step 2: results are identical to step 2. */
#include <string.h>

#include "common.h"

static inline int min_int(int a, int b)
{
	return a < b ? a : b;
}

void matmul_v3_blocked(const float *A, const float *B, float *C, int n)
{
	memset(C, 0, sizeof(float) * (size_t)n * (size_t)n);

	for (int ii = 0; ii < n; ii += BLOCK) {
		int i_end = min_int(ii + BLOCK, n);

		for (int kk = 0; kk < n; kk += BLOCK) {
			int k_end = min_int(kk + BLOCK, n);

			for (int jj = 0; jj < n; jj += BLOCK) {
				int j_end = min_int(jj + BLOCK, n);

				for (int i = ii; i < i_end; i++) {
					for (int k = kk; k < k_end; k++) {
						float a = A[i * n + k];

						for (int j = jj; j < j_end; j++)
							C[i * n + j] += a * B[k * n + j];
					}
				}
			}
		}
	}
}
