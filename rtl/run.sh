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
        -f rtl/filelists/dual_ooo.f
    verilator --lint-only --assert -Wall \
        -Wno-UNUSEDSIGNAL -Wno-UNUSEDPARAM \
        --top-module tiny5_dual_ooo_retire \
        -f rtl/filelists/dual_ooo_retire.f
)

cmake -S "$project_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTING=ON \
    -DTINY5_BUILD_RTL=ON
cmake --build "$build_dir" \
    --target rtl_single_inorder_tests rtl_dual_inorder_tests rtl_dual_ooo_tests rtl_dual_ooo_retire_tests \
        rtl_add_mul_waveform rtl_dual_add_mul_waveform rtl_dual_ooo_add_mul_waveform \
        rtl_dual_ooo_retire_waveform \
        rtl_library_tests_build rtl_branch_protocol_tests_build --parallel 2
ctest --test-dir "$build_dir" --verbose \
    -R '^rtl_' \
    --output-on-failure
printf 'Waveforms: %s %s %s\n' "$build_dir/rtl/add_mul.fst" "$build_dir/rtl/add_mul_dual.fst" "$build_dir/rtl/add_mul_dual_ooo.fst"
printf '           %s %s %s\n' "$build_dir/rtl/queued_alu.fst" "$build_dir/rtl/queued_alu_dual.fst" "$build_dir/rtl/queued_alu_dual_ooo.fst"
printf '           %s %s %s\n' "$build_dir/rtl/queued_alu_chain.fst" "$build_dir/rtl/queued_alu_chain_dual.fst" "$build_dir/rtl/queued_alu_chain_dual_ooo.fst"
printf '           %s %s %s\n' "$build_dir/rtl/queued_alu_hazards.fst" "$build_dir/rtl/queued_alu_hazards_dual.fst" "$build_dir/rtl/queued_alu_hazards_dual_ooo.fst"
for scenario in branch_slow_not_taken_speculation branch_not_taken_divide_alu_chains \
    branch_independent_taken_redirect \
    branch_taken_bypasses_dependency branch_taken_discards_wrong_path \
    branch_backward_loop branch_fast_not_taken_no_gain; do
    printf '           %s %s %s %s\n' "$build_dir/rtl/$scenario.fst" \
        "$build_dir/rtl/${scenario}_dual.fst" "$build_dir/rtl/${scenario}_dual_ooo.fst" \
        "$build_dir/rtl/${scenario}_dual_ooo_retire.fst"
done
