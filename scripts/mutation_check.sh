#!/usr/bin/env bash
# Sanity check for the benchmark's correctness gate: break a version in a COPY (skip the last
# column) and confirm the harness reports WRONG RESULT and exits non-zero.
set -uo pipefail

SRC=$(cd "$(dirname "$0")/.." && pwd)
COPY=$(mktemp -d)
cp -r "$SRC/src" "$SRC/Makefile" "$COPY/"
sed -i 's/for (int j = 0; j < n; j++)\n\t\t\t\tC/&/; /C\[i \* n + j\] += a/ s/^/\t/' "$COPY/src/cpu/v2_reorder.c"
# make the inner j loop stop one short
sed -i 's/for (int j = 0; j < n; j++)$/for (int j = 0; j < n - 1; j++)/' "$COPY/src/cpu/v2_reorder.c"
echo "mutated loop bounds: $(grep -c 'j < n - 1' "$COPY/src/cpu/v2_reorder.c")"

(cd "$COPY" && make -s && bin/bench 256)
rc=$?
rm -rf "$COPY"
if [ $rc -ne 0 ]; then echo "MUTATION KILLED: the correctness gate caught the wrong version"; else echo "MUTATION SURVIVED: gate did not notice"; exit 1; fi
