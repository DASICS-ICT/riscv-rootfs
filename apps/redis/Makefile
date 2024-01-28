NAME = redis-server


include $(RISCV_ROOTFS_HOME)/Makefile.app


REPO_PATH = repo
$(REPO_PATH):
	mkdir -p $@
	git clone --depth 1 git@github.com:DASICS-ICT/DASICS-CVE-2022-24834.git $@




$(APP): | $(REPO_PATH)
	CC=riscv64-unknown-linux-gnu-gcc CXX=riscv64-unknown-linux-gnu-g++ $(MAKE) -C $(REPO_PATH) all -j`nproc`
	cp $(REPO_PATH)/build/* ./build/
	riscv64-unknown-linux-gnu-objdump -d ./build/redis-server > ./build/redis-server.txt