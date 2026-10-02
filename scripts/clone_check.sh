#!/usr/bin/env bash
# Test the COMMITTED state like a new user would see it: fresh clone, build, run a quick benchmark.
# Catches missing files, missing executable bits and broken build rules. Takes about a minute.
set -euo pipefail

SRC=$(cd "$(dirname "$0")/.." && pwd)
CLONE=$(mktemp -d)
trap 'rm -rf "$CLONE"' EXIT

git clone -q "$SRC" "$CLONE/repo"
cd "$CLONE/repo"

for f in scripts/*.sh; do
  [ -x "$f" ] || { echo "NOT EXECUTABLE in clone: $f"; exit 126; }
done

make -s
bin/bench 256 | grep -E 'version|v[1-5]_'
echo "clone check passed: builds, runs, all versions correct on n=256"
