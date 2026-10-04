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
    verilator --lint-only --assert -Wall \
        -Wno-UNUSEDSIGNAL -Wno-UNUSEDPARAM \
        --top-module tiny5_dual_inorder \
        -f rtl/filelists/dual_inorder.f
    verilator --lint-only --assert -Wall \
        -Wno-UNUSEDSIGNAL -Wno-UNUSEDPARAM \
        --top-module tiny5_dual_ooo \
        -f rtl/filelists/common.f rtl/tops/tiny5_dual_ooo.sv
)

cmake -S "$project_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    -DTINY5_BUILD_RTL=ON
cmake --build "$build_dir" \
    --target rtl_single_inorder_tests rtl_dual_inorder_tests rtl_dual_ooo_tests \
        rtl_add_mul_waveform rtl_dual_add_mul_waveform rtl_dual_ooo_add_mul_waveform --parallel 2
ctest --test-dir "$build_dir" --verbose \
    -R '^(rtl_single_inorder|rtl_dual_inorder|rtl_dual_ooo|rtl_add_mul_waveform|rtl_dual_add_mul_waveform|rtl_dual_ooo_add_mul_waveform|rtl_queued_alu_waveform|rtl_dual_queued_alu_waveform|rtl_dual_ooo_queued_alu_waveform|rtl_queued_alu_chain_waveform|rtl_dual_queued_alu_chain_waveform|rtl_dual_ooo_queued_alu_chain_waveform|rtl_queued_alu_hazards_waveform|rtl_dual_queued_alu_hazards_waveform|rtl_dual_ooo_queued_alu_hazards_waveform)$' \
    --output-on-failure
printf 'Waveforms: %s %s %s\n' "$build_dir/rtl/add_mul.fst" "$build_dir/rtl/add_mul_dual.fst" "$build_dir/rtl/add_mul_dual_ooo.fst"
printf '           %s %s %s\n' "$build_dir/rtl/queued_alu.fst" "$build_dir/rtl/queued_alu_dual.fst" "$build_dir/rtl/queued_alu_dual_ooo.fst"
printf '           %s %s %s\n' "$build_dir/rtl/queued_alu_chain.fst" "$build_dir/rtl/queued_alu_chain_dual.fst" "$build_dir/rtl/queued_alu_chain_dual_ooo.fst"
printf '           %s %s %s\n' "$build_dir/rtl/queued_alu_hazards.fst" "$build_dir/rtl/queued_alu_hazards_dual.fst" "$build_dir/rtl/queued_alu_hazards_dual_ooo.fst"
