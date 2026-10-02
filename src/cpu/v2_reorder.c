/* Step 2: same math, loop order i, k, j.
 *
 * Now the inner loop (j) walks B[k][j] and C[i][j] along a row: consecutive floats,
 * so every cache line that is loaded gets fully used. A[i][k] does not change in the
 * inner loop, so it stays in a register.
 * Only the ORDER changed; the arithmetic is identical. */
#include <string.h>

#include "common.h"

void matmul_v2_reorder(const float *A, const float *B, float *C, int n)
{
	memset(C, 0, sizeof(float) * (size_t)n * (size_t)n);	/* this order accumulates into C */

	for (int i = 0; i < n; i++) {
		for (int k = 0; k < n; k++) {
			float a = A[i * n + k];

			for (int j = 0; j < n; j++)
				C[i * n + j] += a * B[k * n + j];
		}
	}
}
