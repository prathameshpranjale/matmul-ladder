#!/usr/bin/env bash
# Produce the numbers reported in the README and the presentation, and keep the raw output in
# bench/history/ with a timestamp. Takes several minutes.
set -euo pipefail

cd "$(dirname "$0")/.."
STAMP=$(date +%Y%m%d-%H%M%S)
H=bench/history; mkdir -p "$H"
make -s

{
  echo "# final run $STAMP   machine: $(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ //')"
  echo "## all versions, n=256 512 1024"
  bin/bench --json bench/results.json 256 512 1024
  echo
  echo "## n=2048, versions 2 to 5"
  bin/bench --only v2,v3,v4,v5 --json bench/results_2048.json 2048
  echo
  echo "## thread scaling, n=1024"
  scripts/thread_scaling.sh 1024 3
  echo
  echo "## thread scaling, n=2048"
  scripts/thread_scaling.sh 2048 3
} 2>&1 | tee "$H/$STAMP-final.txt"

echo "saved $H/$STAMP-final.txt"
