#!/usr/bin/env bash
# Choose PBLOCK (output tile edge per thread, step 5) from measurements. Rotates through the sizes
# every round and reports the median. usage: sweep_pblock.sh [size] [rounds]   env: PBLOCKS="32 64 128 256" THREADS=12
set -euo pipefail

cd "$(dirname "$0")/.."
N=${1:-1024}; ROUNDS=${2:-3}
PBLOCKS=${PBLOCKS:-"32 64 128 256"}
THREADS=${THREADS:-12}

for p in $PBLOCKS; do make -s PBLOCK="$p" BIN="bin/bench_p$p"; done

declare -A vals
for r in $(seq "$ROUNDS"); do
  for p in $PBLOCKS; do
    g=$(OMP_NUM_THREADS=$THREADS QUIET_META=1 "bin/bench_p$p" --only v5_threads "$N" | awk '$1=="v5_threads" {print $4}')
    vals[$p]+="$g "
  done
  echo "round $r done"
done

echo
echo "step 5 at n=$N with $THREADS threads, by PBLOCK:"
for p in $PBLOCKS; do
  echo "${vals[$p]}" | tr ' ' '\n' | grep -v '^$' | sort -n | awk -v p="$p" '{a[NR]=$1} END {printf "  PBLOCK=%-4s median %s  range %s .. %s\n", p, a[int((NR+1)/2)], a[1], a[NR]}'
done
