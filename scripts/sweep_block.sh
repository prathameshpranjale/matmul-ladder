#!/usr/bin/env bash
# Choose the tile size for step 3 from measurements. The machine's speed drifts over time, so each
# tile size is measured in several ROUNDS, rotating through all sizes every round, and we report
# the median. Usage: sweep_block.sh [size]      env: BLOCKS="32 64 128 256"  ROUNDS=3
set -euo pipefail

cd "$(dirname "$0")/.."
N=${1:-1024}
PIN=${PIN:-2}
BLOCKS=${BLOCKS:-"16 32 64 96 128 256"}
ROUNDS=${ROUNDS:-3}

run() { PIN="$PIN" QUIET_META=1 "$@"; }	# bench pins itself

for b in $BLOCKS; do make -s BLOCK="$b" BIN="bin/bench_b$b"; done

declare -A vals
for r in $(seq "$ROUNDS"); do
  for b in $BLOCKS; do
    g=$(run "bin/bench_b$b" --only v3 "$N" | awk '$1=="v3_blocked" {print $4}')
    vals[$b]+="$g "
  done
  echo "round $r done"
done

echo
echo "step 3 at n=$N, median of $ROUNDS rounds:"
for b in $BLOCKS; do
  list=$(echo "${vals[$b]}" | tr ' ' '\n' | grep -v '^$' | sort -n)
  med=$(echo "$list" | awk '{a[NR]=$1} END {print a[int((NR+1)/2)]}')
  printf '  BLOCK=%-4s median %s GFLOPS   all: %s\n' "$b" "$med" "$(echo $list)"
done
