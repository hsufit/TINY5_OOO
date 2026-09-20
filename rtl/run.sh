#!/usr/bin/env bash
set -euo pipefail

rtl_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(dirname -- "$rtl_dir")"
build_dir="${1:-$rtl_dir/build}"

if [[ $# -gt 1 ]]; then
    printf 'Usage: %s [build-directory]\n' "$0" >&2
    exit 2
fi

(
    cd -- "$project_dir"
    verilator --lint-only --assert -Wall \
        -Wno-UNUSEDSIGNAL -Wno-UNUSEDPARAM \
        --top-module tiny5_single_inorder \
        -f rtl/filelists/single_inorder.f
)

cmake -S "$project_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    -DTINY5_BUILD_RTL=ON
cmake --build "$build_dir" --target rtl_single_inorder_tests --parallel 2
ctest --test-dir "$build_dir" --verbose \
    -R '^rtl_single_inorder$' --output-on-failure
