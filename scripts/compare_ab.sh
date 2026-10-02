#!/usr/bin/env bash
# Compare two versions fairly on a machine whose speed drifts: run them in separate bench
# invocations, alternating, several rounds, and print each round plus the median per version.
# usage: compare_ab.sh <size> <versionA> <versionB> [rounds]     e.g. compare_ab.sh 1024 v2_reorder v3_blocked 6
set -euo pipefail

cd "$(dirname "$0")/.."
N=${1:?size}; A=${2:?versionA}; B=${3:?versionB}; ROUNDS=${4:-6}
PIN=${PIN:-2}
make -s

declare -A vals
for r in $(seq "$ROUNDS"); do
  for v in "$A" "$B"; do
    g=$(PIN="$PIN" QUIET_META=1 bin/bench --only "$v" "$N" | awk -v v="$v" '$1==v {print $4}')
    vals[$v]+="$g "
    printf 'round %d  %-12s %s GFLOPS\n' "$r" "$v" "$g"
  done
done

for v in "$A" "$B"; do
  med=$(echo "${vals[$v]}" | tr ' ' '\n' | grep -v '^$' | sort -n | awk '{a[NR]=$1} END {print a[int((NR+1)/2)]}')
  min=$(echo "${vals[$v]}" | tr ' ' '\n' | grep -v '^$' | sort -n | head -1)
  max=$(echo "${vals[$v]}" | tr ' ' '\n' | grep -v '^$' | sort -n | tail -1)
  printf '%-12s median %s   range %s .. %s GFLOPS\n' "$v" "$med" "$min" "$max"
done
