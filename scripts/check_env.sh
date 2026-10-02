#!/usr/bin/env bash
# Check what this machine has for the matmul ladder. Run inside Linux/WSL. Prints facts; never installs.
# Anything marked MISSING is explained at the end with what it blocks.

ok()   { printf '  %-22s %s\n' "$1" "$2"; }
miss() { printf '  %-22s MISSING  (%s)\n' "$1" "$2"; MISSING+=("$1"); }
MISSING=()

echo "== CPU =="
model=$(grep -m1 'model name' /proc/cpuinfo | cut -d: -f2- | sed 's/^ //')
ok "model" "$model"
ok "logical CPUs" "$(nproc)"
flags=$(grep -m1 '^flags' /proc/cpuinfo)
for f in avx avx2 fma avx512f; do
  case " $flags " in *" $f "*) ok "$f" "yes";; *) ok "$f" "no";; esac
done
if command -v lscpu >/dev/null; then
  lscpu | grep -E 'L1d|L2|L3' | sed 's/^/  /'
fi
ok "memory (MB)" "$(free -m | awk '/Mem:/ {print $2}')"

echo "== Compiler and build tools =="
for t in gcc g++ make python3; do
  if command -v "$t" >/dev/null; then ok "$t" "$("$t" --version 2>&1 | head -1)"; else miss "$t" "needed to build/run"; fi
done
# OpenMP support in gcc?
if command -v gcc >/dev/null; then
  if echo 'int main(){return 0;}' | gcc -fopenmp -x c - -o /tmp/omp_test 2>/dev/null; then ok "OpenMP (gcc)" "works"; else miss "OpenMP (gcc)" "needed for the threaded step"; fi
  if echo 'int main(){return 0;}' | gcc -mavx2 -mfma -x c - -o /tmp/avx_test 2>/dev/null; then ok "gcc -mavx2 -mfma" "accepted"; else miss "gcc -mavx2 -mfma" "needed for the SIMD step"; fi
  rm -f /tmp/omp_test /tmp/avx_test
fi

echo "== Profiling =="
if command -v perf >/dev/null; then
  if perf stat -e cycles true >/dev/null 2>&1; then ok "perf" "works (hardware counters)"
  elif perf stat -e task-clock true >/dev/null 2>&1; then ok "perf" "installed, software counters only (no cache-miss counts)"
  else ok "perf" "installed but not usable here"; fi
else
  miss "perf" "cache-miss counts for the CPU steps"
fi

echo "== Reference libraries =="
if dpkg -s libopenblas-dev >/dev/null 2>&1; then ok "OpenBLAS" "installed"; else miss "OpenBLAS" "CPU reference to compare against (libopenblas-dev)"; fi

echo "== GPU / CUDA =="
smi=$(command -v nvidia-smi || ls /usr/lib/wsl/lib/nvidia-smi 2>/dev/null)
if [ -n "$smi" ]; then
  ok "nvidia-smi" "found"
  "$smi" --query-gpu=name,driver_version,memory.total,compute_cap --format=csv,noheader 2>/dev/null | sed 's/^/  GPU: /'
else
  miss "nvidia-smi" "GPU not visible from this Linux (needs the Windows NVIDIA driver with WSL support)"
fi
nvcc=$(command -v nvcc || ls /usr/local/cuda/bin/nvcc 2>/dev/null)
if [ -n "$nvcc" ]; then ok "nvcc" "$("$nvcc" --version | tail -1)"; else miss "nvcc" "CUDA toolkit (needed to compile the GPU steps)"; fi
if dpkg -s libcublas-dev-12-0 >/dev/null 2>&1 || ls /usr/local/cuda/lib64/libcublas.so >/dev/null 2>&1 || ls /usr/local/cuda*/lib64/libcublas.so >/dev/null 2>&1; then
  ok "cuBLAS" "found"
else
  miss "cuBLAS" "GPU reference to compare against (ships with the CUDA toolkit)"
fi

echo
if [ ${#MISSING[@]} -eq 0 ]; then
  echo "All checks passed."
else
  echo "Missing: ${MISSING[*]}"
  exit 1
fi
