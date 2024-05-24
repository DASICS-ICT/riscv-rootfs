SPEC = /home/wanghan/Workspace/DASICS_ICT/DASICS-SPEC/spec2006-lite
LIBDASICS=/home/wanghan/Workspace/DASICS_ICT/DASICS-SPEC/LibDASICS
CROSS_COMPILE=riscv64-unknown-linux-gnu-
SPECINT = 400.perlbench \
			401.bzip2 \
			403.gcc \
			429.mcf \
			445.gobmk \
			456.hmmer \
			458.sjeng \
			462.libquantum \
			464.h264ref \
			471.omnetpp \
			473.astar \
			483.xalancbmk


all: dd


dd: 
	@for file in $(SPECINT); do \
		$(MAKE) -C $(SPEC)/$$file; \
		suffix=$$(echo $$file | rev | cut -d. -f1 | rev); \
		mkdir -p $$suffix; \
		cp $(SPEC)/$$file/build/$$suffix ./$$suffix; \
		riscv64-unknown-linux-gnu-strip ./$$suffix/$$suffix; \
		cp -r $(SPEC)/spec-all/cpu2006_run_dir/$$suffix/* ./$$suffix; \
	done


clean: 
	@for file in $(SPECINT); do \
		suffix=$$(echo $$file | rev | cut -d. -f1 | rev); \
		rm $$suffix; \
	done