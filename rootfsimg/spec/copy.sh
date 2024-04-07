SPEC=/home/wanghan/Workspace/DASICS_ICT/DASICS-SPEC/spec2006-lite

make -C ${SPEC}/401.bzip2/ clean
make -C ${SPEC}/401.bzip2/ -j16

make -C ${SPEC}/403.gcc/ clean
make -C ${SPEC}/403.gcc/ -j16 

make -C ${SPEC}/429.mcf/ clean
make -C ${SPEC}/429.mcf/ -j16

make -C ${SPEC}/445.gobmk/ clean
make -C ${SPEC}/445.gobmk/ -j16

make -C ${SPEC}/456.hmmer/ clean
make -C ${SPEC}/456.hmmer/ -j16

make -C ${SPEC}/462.libquantum/ clean
make -C ${SPEC}/462.libquantum/ -j16

make -C ${SPEC}/471.omnetpp/ clean
make -C ${SPEC}/471.omnetpp/ -j16

make -C ${SPEC}/458.sjeng/ clean
make -C ${SPEC}/458.sjeng/ -j16

cp ${SPEC}/401.bzip2/build/bzip2 ./
cp ${SPEC}/403.gcc/build/gcc ./
cp ${SPEC}/429.mcf/build/mcf ./
cp ${SPEC}/445.gobmk/build/gobmk ./
cp ${SPEC}/456.hmmer/build/hmmer ./
cp ${SPEC}/462.libquantum/build/libquantum ./
cp ${SPEC}/471.omnetpp/build/omnetpp ./
cp ${SPEC}/458.sjeng/build/sjeng ./




