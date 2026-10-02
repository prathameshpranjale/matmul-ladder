/* Step 0: the reference. Slow and simple on purpose: its only job is to be right.
 * It accumulates in double so the float versions have an accurate answer to be checked against.
 *
 * Loop order is i, k, j (not the textbook i, j, k) only so big matrices do not take minutes to
 * check. For every element the k terms are still added in ascending order into a double, so the
 * result is exactly the same as the textbook order. */
#include <string.h>

#include "common.h"

void matmul_reference(const float *A, const float *B, double *Cref, int n)
{
	memset(Cref, 0, sizeof(double) * (size_t)n * (size_t)n);

	for (int i = 0; i < n; i++) {
		for (int k = 0; k < n; k++) {
			double a = (double)A[i * n + k];

			for (int j = 0; j < n; j++)
				Cref[i * n + j] += a * (double)B[k * n + j];
		}
	}
}
