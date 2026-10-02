/* Step 1: the textbook triple loop, order i, j, k.
 *
 * The inner loop reads A along a row (A[i][k], k+1 is the next float: friendly),
 * but reads B down a column (B[k][j], k+1 jumps a whole row of n floats: unfriendly).
 * Each B access lands on a different cache line, so for large n most B reads miss the cache. */
#include "common.h"

void matmul_v1_naive(const float *A, const float *B, float *C, int n)
{
	for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++) {
			float sum = 0.0f;

			for (int k = 0; k < n; k++)
				sum += A[i * n + k] * B[k * n + j];
			C[i * n + j] = sum;
		}
	}
}
