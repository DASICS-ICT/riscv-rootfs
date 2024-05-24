#!/bin/sh

echo '===== Start running SPEC2006 ====='
echo '======== BEGIN mcf ========'
set -x
date -R

cd /spec/mcf && time ./mcf inp.in -dasics

date -R
set +x
echo '======== END   mcf ========'
md5sum inp.out mcf.out
echo '======== BEGIN bzip2_chicken ========'
set -x
date -R

cd /spec/bzip2 && time ./bzip2 chicken.jpg 30 -dasics 

date -R
set +x
echo '======== END   bzip2_chicken ========'
md5sum chicken.jpg.out


echo '======== BEGIN gobmk_nngs ========'
set -x
date -R

cd /spec/gobmk && time ./gobmk --quiet --mode gtp < nngs.tst -dasics 

date -R
set +x
echo '======== END   gobmk_nngs ========'
echo '======== BEGIN hmmer_nph3 ========'
set -x
date -R

cd /spec/hmmer && time ./hmmer nph3.hmm swiss41 -dasics 

date -R
set +x
echo '======== END   hmmer_nph3 ========'
echo '======== BEGIN libquantum ========'
set -x
date -R

cd /spec/libquantum && time ./libquantum 1397 8 -dasics 

date -R
set +x
echo '======== END   libquantum ========'

echo '======== BEGIN sjeng ========'
set -x
date -R

cd /spec/sjeng && time ./sjeng ref.txt -dasics 

date -R
set +x
echo '======== END   sjeng ========'


echo '======== BEGIN gcc ========'
set -x
date -R

cd /spec/gcc && time ./gcc 200.i -o 200.s -dasics 

date -R
set +x
echo '======== END   gcc ========'



echo '======== BEGIN perlbench DASICS ========'
set -x
date -R

cd /spec/perlbench && time ./perlbench -I./lib diffmail.pl 4 800 10 17 19 300 -dasics

date -R
set +x
echo '======== END   perlbench DASICS ========'



echo '======== BEGIN h264ref DASICS ========'
set -x
date -R

cd /spec/h264ref && time ./h264ref -d foreman_ref_encoder_main.cfg -dasics

date -R
set +x
echo '======== END   h264ref DASICS ========'

echo '======== BEGIN astar DASICS ========'
set -x
date -R

cd /spec/astar && time ./astar rivers.cfg -dasics

date -R
set +x
echo '======== END   astar DASICS ========'


echo '======== BEGIN xalancbmk DASICS ========'
set -x
date -R

cd /spec/xalancbmk && time ./xalancbmk  -v t5.xml xalanc.xsl -dasics

date -R
set +x
echo '======== END   xalancbmk DASICS ========'


echo '======== BEGIN omnetpp DASICS ========'
set -x
date -R

cd /spec/omnetpp && time ./omnetpp omnetpp.ini -dasics

date -R
set +x
echo '======== END   omnetpp DASICS ========'

/spec/trap
