#!/bin/sh
echo '===== Start running SPEC2006 ====='
echo '======== DASICS BEGIN sjeng ========'
set -x
date -R

cd /spec && time ./sjeng_base.riscv64-unknown-linux-gnu-gcc-10.2.0 ref.txt -dasics

date -R
set +x
echo '======== DASICS END   sjeng ========'
echo '===== Finish running SPEC2006 ====='
/spec_common/trap
