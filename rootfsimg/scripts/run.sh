#!/bin/sh
# echo '===== Start running SPEC2006 ====='
# echo '======== BEGIN mcf ========'
# set -x
# date -R

# cd /spec && time ./mcf inp.in -dasics

# date -R
# set +x
# echo '======== END   mcf ========'
# md5sum inp.out mcf.out
# echo '======== BEGIN bzip2_chicken ========'
# set -x
# date -R

# cd /spec && time ./bzip2 chicken.jpg 30 -dasics 

# date -R
# set +x
# echo '======== END   bzip2_chicken ========'
# md5sum chicken.jpg.out
# echo '======== BEGIN gcc_200 ========'
# set -x
# date -R

# cd /spec && time ./gcc 200.i -o 200.s -dasics 

# date -R
# set +x
# echo '======== END   gcc_200 ========'
# md5sum 200.s
# echo '======== BEGIN gobmk_nngs ========'
# set -x
# date -R

# cd /spec && time ./gobmk --quiet --mode gtp < nngs.tst -dasics 

# date -R
# set +x
# echo '======== END   gobmk_nngs ========'
# md5sum nngs.out
echo '======== BEGIN hmmer_nph3 ========'
set -x
date -R

cd /spec && time ./hmmer nph3.hmm swiss41 -dasics 

date -R
set +x
echo '======== END   hmmer_nph3 ========'
md5sum nph3.out
echo '======== BEGIN libquantum ========'
set -x
date -R

cd /spec && time ./libquantum 1397 8 -dasics 

date -R
set +x
echo '======== END   libquantum ========'
md5sum ref.out
echo '======== BEGIN omnetpp ========'
set -x
date -R

cd /spec && time ./omnetpp omnetpp.ini -dasics 

date -R
set +x
echo '======== END   omnetpp ========'
md5sum omnetpp.log omnetpp.sca
echo '======== BEGIN sjeng ========'
set -x
date -R

cd /spec && time ./sjeng ref.txt -dasics 

date -R
set +x
echo '======== END   sjeng ========'
md5sum ref.out
echo '===== Finish running SPEC2006 ====='
/spec/trap