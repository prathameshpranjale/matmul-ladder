#!/usr/bin/env bash
# Build and run the CPU benchmark; writes bench/results.json. Extra args are matrix sizes.
set -euo pipefail

cd "$(dirname "$0")/.."
make -s

# bench pins the single-thread versions to one core itself (PIN, default 2 = a performance-core
# thread on this machine) and leaves the threaded version free. Do NOT wrap this in taskset.
bin/bench --json bench/results.json "$@"
echo "wrote bench/results.json"
