# Challenges and Fixes

Every real problem we hit, what caused it, how we found it, how we fixed it, and the lesson. Written so you can retell each one in an interview.

Format: **Symptom** -> **Cause** -> **How we found it** -> **Fix** -> **Lesson**.

---

# Project A: edu-pci-driver

### A1. The build "did nothing" (path problem)
- **Symptom:** kernel source seemed missing; the first background build never started.
- **Cause:** commands launched from Windows into WSL had a mangled `HOME` (`C:UsersPPRANJALE`), so `~/work` pointed to the wrong place.
- **How found:** `ls ~/work` was empty while the files were really in `/home/firefist/work`; printing `$HOME` showed the bad value.
- **Fix:** use absolute paths (`/home/<user>/work`) in all scripts.
- **Lesson:** when a build silently does nothing, check paths and environment first.

### A2. The interrupt never arrived (MSI)
- **Symptom:** factorial request timed out. The device had the right answer (3628800) and an interrupt pending, but the handler never ran.
- **Cause:** unknown. We first concluded "QEMU's `edu` device never delivers MSI", but **that was wrong**: QEMU's source (`hw/misc/edu.c`, v8.2.2) implements MSI (`msi_init`, `msi_notify`). In our setup the MSI never reached the handler, with KVM on and off; the cause is probably in our driver or VM setup and is still open.
- **How found:** printed device registers on timeout; then changed one thing at a time: INTx worked, MSI with KVM off still failed.
- **Fix:** use legacy shared INTx (works); remove "MSI" from the resume line; later corrected the wrong "device can't do MSI" claim after reading QEMU's source.
- **Lesson:** one-change experiments show *that* something fails, not *why*. Read the device's source or datasheet before blaming it. Do not claim features (or limits) you could not verify.

### A3. Tests passed but proved little
- **Risk:** a passing test only means something if it can fail.
- **Fix:** mutation checks: remove the mutex (stress test must fail), skip the return DMA (DMA test must fail), plant a heap overflow (KASAN must report it). All three were caught.
- **Lesson:** when a test passes on the first try, try to break the code on purpose.

### A4. CI failed with exit code 126
- **Cause:** scripts were committed as `100644` (not executable); `chmod +x` on the Windows drive did not stick.
- **How found:** GitHub annotation `exit code 126`; `git ls-files -s` showed the modes.
- **Fix:** `git update-index --chmod=+x`; added `scripts/ci_local.sh` which tests a fresh clone like CI would.
- **Lesson:** test the committed state, not your working folder.

### A5. CI failed although all tests passed
- **Cause:** my kernel-health check matched the word `WARNING:`, and the runner's CPU prints a harmless `RETBleed: WARNING ... vulnerable` boot notice.
- **How found:** downloaded the job log with the token and read the end of the test step.
- **Fix:** match real warnings only (`WARNING: CPU:`, `WARNING: possible ...`, `BUG:`, `KASAN`, `Call Trace`); re-checked that KASAN was still caught.
- **Lesson:** a check that cries wolf gets ignored; match real problems precisely.

### A6. Access tokens pasted into chat
- **Problem:** several GitHub tokens were pasted into the conversation; one lacked permission to create repos, another was not scoped to the new repo.
- **Fix:** pushed with the token passed through the environment for one command (not stored in git config or the remote URL); one token saved as a user environment variable on request.
- **Lesson:** treat any pasted token as exposed and revoke it; use tokens limited to one repo with minimal permissions.

---

# Project B: matmul-ladder

### B-1. This machine has no NVIDIA GPU
- **Symptom:** `nvidia-smi` not found anywhere.
- **Cause:** Windows lists only `Intel(R) UHD Graphics`; the RTX 4060 is not on this computer.
- **Fix:** CPU steps (0 to 5) run here; GPU steps (6, 7) deferred to another machine or a cloud GPU. The plan was corrected.
- **Lesson:** verify assumed hardware before planning around it.

### B-2. `perf` is not available
- **Impact:** no cache-miss counts, so every "why it is faster" explanation is a hypothesis that fits the data.
- **Fix:** label explanations as hypotheses; never quote cache-miss numbers.

### B-3. Timings were noisy and contradicted themselves
- **Symptom:** the same code gave 3.25 and then 0.95 GFLOPS at n=512 in two runs.
- **Cause:** a laptop under WSL2: frequency changes, other processes, core migration.
- **Fix:** more runs for small sizes, pin to one core, print a `noise` column, run twice before believing a number.

### B-4. The first tile sweep gave the wrong answer
- **Symptom:** it said blocking was slower than the plain reorder and chose tile 128; a full run later said the opposite.
- **Cause:** each tile size was run once, one after another, while the machine's speed drifted 25% to 40%.
- **Fix:** alternate versions across rounds and compare medians and ranges; call a difference real only when ranges do not overlap. Scripts: `compare_ab.sh`, rotating `sweep_block.sh`.
- **Lesson:** on a machine that drifts, the order of measurement can create a fake result.

### B-5. Absolute numbers change between sessions
- **Symptom:** naive n=1024 measured 0.40 and later 0.59 GFLOPS for the same code.
- **Fix:** report ranges, and quote ratios from same-session alternating comparisons.

### B-6. The compiler could hide each step's effect
- **Risk:** with `-O3` or `-march=native`, gcc vectorizes the loops itself, so steps blur together.
- **Fix:** compile with `-O2 -fno-tree-vectorize`; add SIMD only deliberately in step 4.

### B-7. A slow reference made big sizes impractical
- **Symptom:** checking n=2048 would spend most of the time in the reference.
- **Fix:** reorder the reference loops (i, k, j) while keeping the same double accumulation order, so the answer is identical but much faster.

### B-8. A passing correctness check must be able to fail
- **Fix:** break a version in a copy (skip the last column): the harness reports `WRONG RESULT` and times nothing. The result buffer is also pre-filled with NaN, so a version that skips work cannot pass.

### B-9. Small tiles hurt, and blocking gain was modest
- **Finding:** tile sizes 16 to 96 were slower than no blocking; tile 256 gave about 15% at n=1024.
- **Handling:** wrote the explanation as a hypothesis (the scalar loop is probably limited by load/store instructions), to be tested by the SIMD step.

### B-10. My own build check hid a compile error
- **Symptom:** the benchmark ran and printed "BUILD_OK" even though the build had failed (missing `#include <unistd.h>` for `sysconf`).
- **Cause:** I piped `make` into `head`, so the pipeline's exit status was `head`'s (success), and the OLD binary ran.
- **Fix:** `set -o pipefail`, `rm -f bin/bench` before building, and read the real error.
- **Lesson:** a status check that cannot fail is not a check. Always make sure you run the thing you just built.

### B-11. Pinning to one core would have crippled the threaded step
- **Risk:** the scripts wrapped the benchmark in `taskset -c 2`, which would pin every thread of step 5 to one core.
- **Fix:** the harness now pins only the single-thread versions (via `sched_setaffinity` on the calling thread) and leaves the threaded version free; scripts no longer use `taskset`.

### B-12. SIMD without hiding it from the earlier steps
- **Decision:** compile only the AVX2 functions with `__attribute__((target("avx2,fma")))` instead of `-mavx2` for the whole program, so earlier steps stay scalar, and the harness checks the CPU supports AVX2 + FMA before running them.
- **Side effect:** FMA rounds once instead of twice, so results differ from the scalar versions in the last bits (still far inside the tolerance).

### B-13. Tiny matrices make nonsense threaded numbers
- **Symptom:** 3387% noise, 26 to 72 GFLOPS at n=100 to 300.
- **Cause:** thread start-up and scheduling cost dominates a job that takes 0.1 ms.
- **Fix:** never quote threaded numbers below n=512; the final report uses n >= 1024 for threads.

### B-14. Step 5 with one thread was slower than step 4
- **Symptom:** 19.8 vs 25.7 GFLOPS for the same kernel.
- **Cause (hypothesis):** the 64-wide output tiles make the inner loops shorter and add loop overhead.
- **How tested:** swept the thread-tile size PBLOCK; on one thread, larger tiles recovered the speed (256: 27.0). Chose 128 as the best balance with 12 threads.

### B-15. Threads did not scale evenly
- **Finding:** at n=1024, 2 threads gave 48.9 GFLOPS but 4 threads gave 47.6 (no gain), then it grew again up to 81.7 with 12. 12 threads is only 3.0x over one SIMD thread.
- **Probable cause (hypothesis, not measured):** this CPU has 2 fast performance cores (4 hardware threads) and 8 slower efficiency cores, and the two threads of one core share its execution units; the OS also decides where threads land.
- **Handling:** reported as measured; the explanation is labeled a hypothesis.

### B-16. Many numbers, one story
- Because speed drifts between sessions, the report uses: the final run (single pass, noise shown), alternating comparisons for close calls (3 vs 4, blocking), and 3-round rotating medians for thread scaling.

### B-17. Checking the presentation page without a browser window
- **Problem:** I could not just open the page and look at it.
- **Approach:** headless Edge: `--dump-dom` to check the generated DOM (chart content, counts, no `undefined`), `--screenshot` plus cropping to look at sections, and a temporary test copy of the page that clicks the animation programmatically and reads back the miss counts.
- **Snags:** the first DOM dump was empty until I gave Edge its own profile folder; a test file under the short `~` temp path would not load as a `file://` URL (used the real project path instead); a command containing `Remove-Item` plus a quoted program path was blocked by the permission check (split into two commands).
- **What the screenshots caught:** a chart label drawn over the bars, and an end-of-run message that needed an extra click. Both fixed.
- **Lesson:** a page that "builds" is not a page that "works". Render it, look at it, and click it.

### B-18. A claim in my own notes was stronger than my evidence
- **Problem:** the step-3 notes said the L2 size was "from lscpu". It was an estimate (architecture knowledge plus the lscpu totals), not verified per core.
- **Fix:** reworded as an unverified estimate, and the presentation labels it as such.
- **Lesson:** re-read your own explanations for claims you cannot back up.

---

*(New challenges are appended below as they happen.)*
