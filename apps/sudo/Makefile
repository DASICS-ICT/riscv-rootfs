NAME = sudo

include $(RISCV_ROOTFS_HOME)/Makefile.app

REPO_PATH = repo
$(REPO_PATH):
	mkdir -p $@
	git clone --depth 1 -b dasics-protection-ecall git@github.com:DASICS-ICT/DASICS-CVE-2021-3156.git $@

$(APP): | $(REPO_PATH)
	$(MAKE) -C $(REPO_PATH) all
	ln -sf $(abspath $(REPO_PATH)/build/sudo) $@
	cp $(REPO_PATH)/build/exploit ./build
	cp $(REPO_PATH)/build/libnss_x-x.so.2 ./build

	
