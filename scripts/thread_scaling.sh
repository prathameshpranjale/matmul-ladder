#!/usr/bin/env bash
# How does step 5 scale with thread count? Each round runs every thread count once (rotating, so
# drift in machine speed hits all counts equally); reports the median and range per thread count.
# Also measures step 4 (one thread, SIMD) as the reference to compare against.
# usage: thread_scaling.sh [size] [rounds]     env: THREADS="1 2 4 6 8 12"
set -euo pipefail

cd "$(dirname "$0")/.."
N=${1:-1024}; ROUNDS=${2:-3}
THREADS=${THREADS:-"1 2 4 6 8 12"}
make -s

declare -A vals
vals[simd1]=""
for r in $(seq "$ROUNDS"); do
  g=$(QUIET_META=1 bin/bench --only v4_simd "$N" | awk '$1=="v4_simd" {print $4}')
  vals[simd1]+="$g "
  for t in $THREADS; do
    g=$(OMP_NUM_THREADS=$t QUIET_META=1 bin/bench --only v5_threads "$N" | awk '$1=="v5_threads" {print $4}')
    vals[$t]+="$g "
  done
  echo "round $r done"
done

stat() { echo "$1" | tr ' ' '\n' | grep -v '^$' | sort -n | awk '{a[NR]=$1} END {printf "median %s  range %s .. %s", a[int((NR+1)/2)], a[1], a[NR]}'; }

echo
echo "n=$N, $ROUNDS rounds, GFLOPS"
printf '  %-22s %s\n' "v4_simd (1 thread)" "$(stat "${vals[simd1]}")"
for t in $THREADS; do
  printf '  %-22s %s\n' "v5_threads, $t thr" "$(stat "${vals[$t]}")"
done
