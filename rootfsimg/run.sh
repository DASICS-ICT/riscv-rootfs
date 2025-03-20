#!/bin/sh
echo '===== Start running SPEC2006 ====='
echo '======== DASICS BEGIN omnetpp ========'
set -x
date -R

cd /spec && time ./omnetpp_base.riscv64-unknown-linux-gnu-gcc-10.2.0 omnetpp.ini -dasics  > result.txt

date -R
set +x
echo '======== DASICS END   omnetpp ========'
echo '===== Finish running SPEC2006 ====='
/spec_common/trap
