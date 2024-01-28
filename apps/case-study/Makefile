NAME = attack-case

include $(RISCV_ROOTFS_HOME)/Makefile.app

REPO_PATH = repo
$(REPO_PATH):
	mkdir -p $@
	git clone --depth 1 git@github.com:DASICS-ICT/DASICS-case-study.git $@

$(APP): | $(REPO_PATH)
	$(MAKE) -C $(REPO_PATH) all
	ln -sf $(abspath $(REPO_PATH)/build/attack-case) $@
