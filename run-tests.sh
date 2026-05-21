#!/usr/bin/env bash

set -euo pipefail

script_dir="$(dirname "$(realpath "$0")")"
build_dir="$script_dir/build/tests"

if [ ! -f "$build_dir/CMakeCache.txt" ]; then
  cmake -S "$script_dir" -B "$build_dir" -DBUILD_TESTS=ON
fi

cmake --build "$build_dir" --target crdt_tests --parallel
ctest --test-dir "$build_dir" --output-on-failure
