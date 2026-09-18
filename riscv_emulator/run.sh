#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
build_dir=${1:-"$script_dir/build"}

cmake -S "$script_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$build_dir" -j2
ctest --test-dir "$build_dir" --output-on-failure
"$build_dir/rv32im_tests"
