#!/usr/bin/env bash
set -euo pipefail

# 1) Unpack all .tar archives into their own dirs
for tarball in *.tar; do
  dir="${tarball%.tar}"
  echo "=== Extracting $tarball → $dir/ ==="
  mkdir -p "$dir"
  tar -xf "$tarball" -C "$dir"
done

# 2) Inside each extracted dir, gunzip any .cnf.gz
find . -type f -name '*.cnf.gz' -print0 | xargs -0 -n1 gunzip -f

# 3) Gather every .cnf under benchmarks/
mapfile -t cnfs < <(find . -type f -name '*.cnf' | sort)

# 4) Run test_gpu_vs_cpu on each
echo
echo "=== Running GPU vs CPU check on all CNFs ==="
for cnf in "${cnfs[@]}"; do
  printf "\n--- %s ---\n" "$cnf"
  ../build/test_gpu_vs_cpu "$cnf"
done
