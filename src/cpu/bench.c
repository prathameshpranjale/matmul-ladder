/* Benchmark harness for the CPU ladder.
 *
 * For each size and each version:
 *   1. run once (warm-up) and CHECK the result against the reference;
 *   2. only if it is correct, time several runs and keep the median;
 *   3. report GFLOPS = 2 * n^3 / seconds.
 *
 * usage: bench [--json path] [--only name,name] [size ...]      default sizes: 256 512 1024
 *   --only: run just the versions whose name contains one of the comma-separated words
 *
 * Single-thread versions are pinned to one CPU (env PIN, default 2; PIN=-1 disables) so the OS does
 * not move them between cores mid-measurement. Threaded versions are un-pinned and use
 * OMP_NUM_THREADS threads.
 */
#define _GNU_SOURCE
#include <math.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#ifdef _OPENMP
#include <omp.h>
#endif

#include "common.h"

#ifndef CFLAGS_STR
#define CFLAGS_STR "unknown"
#endif

#define TOLERANCE 1e-4	/* max |C - ref| divided by max(1, max |ref|) */

struct version {
	const char *name;
	void (*fn)(const float *, const float *, float *, int);
	int threaded;	/* 1: do not pin to a single CPU */
	int needs_avx2;	/* 1: skip on CPUs without AVX2 + FMA */
};

static const struct version versions[] = {
	{ "v1_naive",    matmul_v1_naive,    0, 0 },
	{ "v2_reorder",  matmul_v2_reorder,  0, 0 },
	{ "v3_blocked",  matmul_v3_blocked,  0, 0 },
	{ "v4_simd",     matmul_v4_simd,     0, 1 },
	{ "v5_threads",  matmul_v5_threads,  1, 1 },
};
#define NVERSIONS ((int)(sizeof(versions) / sizeof(versions[0])))

struct result {
	const char *name;
	int n;
	int runs;
	double seconds;	/* median */
	double spread;	/* (slowest - fastest) / median: how noisy this measurement was */
	double gflops;
	double max_err;
	int correct;
};

/* True if no filter was given, or the name contains one of the comma-separated words. */
static int selected(const char *only, const char *name)
{
	char buf[256], *tok, *save;

	if (!only)
		return 1;
	snprintf(buf, sizeof(buf), "%s", only);
	for (tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save))
		if (strstr(name, tok))
			return 1;
	return 0;
}

/* Restrict the calling thread to one CPU (cpu >= 0), or allow every CPU again (cpu < 0). */
static void set_affinity(int cpu)
{
	cpu_set_t set;

	CPU_ZERO(&set);
	if (cpu >= 0) {
		CPU_SET(cpu, &set);
	} else {
		long total = sysconf(_SC_NPROCESSORS_CONF);

		for (long c = 0; c < total && c < CPU_SETSIZE; c++)
			CPU_SET((int)c, &set);
	}
	sched_setaffinity(0, sizeof(set), &set);	/* 0 = the calling thread */
}

static int cpu_has_avx2(void)
{
	__builtin_cpu_init();
	return __builtin_cpu_supports("avx2") && __builtin_cpu_supports("fma");
}

static int thread_count(void)
{
#ifdef _OPENMP
	return omp_get_max_threads();
#else
	return 1;
#endif
}

static double now_sec(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static int cmp_double(const void *a, const void *b)
{
	double x = *(const double *)a, y = *(const double *)b;

	return (x > y) - (x < y);
}

/* Deterministic pseudo-random values in [-1, 1) so every run uses the same input. */
static void fill(float *m, size_t count, unsigned seed)
{
	for (size_t i = 0; i < count; i++) {
		seed = seed * 1664525u + 1013904223u;
		m[i] = (float)((seed >> 8) & 0xFFFF) / 32768.0f - 1.0f;
	}
}

static double max_error(const float *C, const double *ref, size_t count)
{
	double err = 0.0, scale = 1.0;

	for (size_t i = 0; i < count; i++) {
		double d = fabs((double)C[i] - ref[i]);

		if (d > err)
			err = d;
		if (fabs(ref[i]) > scale)
			scale = fabs(ref[i]);
	}
	return err / scale;
}

static void cpu_model(char *out, size_t len)
{
	FILE *f = fopen("/proc/cpuinfo", "r");
	char line[256];

	snprintf(out, len, "unknown");
	if (!f)
		return;
	while (fgets(line, sizeof(line), f)) {
		if (!strncmp(line, "model name", 10)) {
			char *p = strchr(line, ':');

			if (p) {
				p++;
				while (*p == ' ')
					p++;
				p[strcspn(p, "\n")] = '\0';
				snprintf(out, len, "%s", p);
			}
			break;
		}
	}
	fclose(f);
}

static void write_json(const char *path, const struct result *r, int count,
		       const int *sizes, int nsizes)
{
	char cpu[200];
	FILE *f = fopen(path, "w");

	if (!f) {
		perror(path);
		return;
	}
	cpu_model(cpu, sizeof(cpu));
	fprintf(f, "{\n  \"meta\": {\n    \"cpu\": \"%s\",\n    \"compiler\": \"gcc %s\",\n"
		   "    \"cflags\": \"%s\",\n    \"block\": %d,\n    \"threads\": %d,\n    \"sizes\": [",
		cpu, __VERSION__, CFLAGS_STR, BLOCK, thread_count());
	for (int i = 0; i < nsizes; i++)
		fprintf(f, "%s%d", i ? ", " : "", sizes[i]);
	fprintf(f, "],\n    \"metric\": \"GFLOPS = 2*n^3/seconds, median of runs\"\n  },\n  \"results\": [\n");
	for (int i = 0; i < count; i++)
		fprintf(f, "    {\"version\": \"%s\", \"n\": %d, \"runs\": %d, \"seconds\": %.6f, "
			   "\"spread\": %.3f, \"gflops\": %.4f, \"max_err\": %.3e, \"correct\": %s}%s\n",
			r[i].name, r[i].n, r[i].runs, r[i].seconds, r[i].spread, r[i].gflops, r[i].max_err,
			r[i].correct ? "true" : "false", i + 1 < count ? "," : "");
	fprintf(f, "  ]\n}\n");
	fclose(f);
}

int main(int argc, char **argv)
{
	int default_sizes[] = { 256, 512, 1024 };
	int sizes[16], nsizes = 0;
	const char *json_path = NULL, *only = NULL;
	struct result results[16 * NVERSIONS];
	int nresults = 0;
	const char *pin_env = getenv("PIN");
	int pin_cpu = pin_env ? atoi(pin_env) : 2;	/* -1 = do not pin */

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--json") && i + 1 < argc)
			json_path = argv[++i];
		else if (!strcmp(argv[i], "--only") && i + 1 < argc)
			only = argv[++i];
		else if (nsizes < 16 && atoi(argv[i]) > 0)
			sizes[nsizes++] = atoi(argv[i]);
	}
	if (!nsizes) {
		memcpy(sizes, default_sizes, sizeof(default_sizes));
		nsizes = 3;
	}

	printf("%-12s %6s %10s %9s %8s %10s  %s\n", "version", "n", "seconds", "GFLOPS", "noise", "max_err", "check");
	if (!getenv("QUIET_META"))
		printf("# threads for threaded versions: %d   pin cpu for others: %d   block: %d\n",
		       thread_count(), pin_cpu, BLOCK);

	for (int s = 0; s < nsizes; s++) {
		int n = sizes[s];
		size_t count = (size_t)n * (size_t)n;
		float *A = aligned_alloc(64, count * sizeof(float));
		float *B = aligned_alloc(64, count * sizeof(float));
		float *C = aligned_alloc(64, count * sizeof(float));
		double *ref = malloc(count * sizeof(double));
		int runs = n <= 256 ? 15 : n <= 512 ? 11 : 5;	/* more runs where each run is short and noisy */

		if (!A || !B || !C || !ref) {
			fprintf(stderr, "out of memory at n=%d\n", n);
			return 1;
		}
		fill(A, count, 1u);
		fill(B, count, 2u);
		matmul_reference(A, B, ref, n);

		for (int v = 0; v < NVERSIONS; v++) {
			struct result *r;

			double times[16];

			if (!selected(only, versions[v].name))
				continue;
			if (versions[v].needs_avx2 && !cpu_has_avx2()) {
				printf("%-12s skipped: this CPU has no AVX2/FMA\n", versions[v].name);
				continue;
			}
			r = &results[nresults++];

			/* pin single-thread versions; let threaded ones use every CPU */
			set_affinity(versions[v].threaded ? -1 : pin_cpu);

			memset(C, 0xFF, count * sizeof(float));	/* NaN-filled: a version that skips work fails the check */
			versions[v].fn(A, B, C, n);		/* warm-up + correctness run */

			r->name = versions[v].name;
			r->n = n;
			r->runs = runs;
			r->max_err = max_error(C, ref, count);
			r->correct = r->max_err < TOLERANCE;
			r->seconds = r->spread = r->gflops = 0.0;

			if (r->correct) {
				for (int i = 0; i < runs; i++) {
					double t0 = now_sec();

					versions[v].fn(A, B, C, n);
					times[i] = now_sec() - t0;
				}
				qsort(times, (size_t)runs, sizeof(double), cmp_double);
				r->seconds = times[runs / 2];
				r->spread = (times[runs - 1] - times[0]) / r->seconds;
				r->gflops = 2.0 * n * (double)n * n / r->seconds / 1e9;
			}
			printf("%-12s %6d %10.4f %9.3f %7.0f%% %10.2e  %s\n", r->name, n, r->seconds, r->gflops,
			       r->spread * 100.0, r->max_err, r->correct ? "ok" : "WRONG RESULT");
			fflush(stdout);
		}
		free(A); free(B); free(C); free(ref);
	}

	if (json_path)
		write_json(json_path, results, nresults, sizes, nsizes);

	for (int i = 0; i < nresults; i++)
		if (!results[i].correct)
			return 1;
	return 0;
}
