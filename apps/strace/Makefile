NAME = strace

include $(RISCV_ROOTFS_HOME)/Makefile.app

REPO_PATH = repo
TARBALL = strace-6.6.tar.xz
CONFIG_FLAGS = --host=riscv64-unknown-linux-gnu

$(TARBALL):
	curl -O https://strace.io/files/6.6/$(TARBALL)
	
$(REPO_PATH): $(TARBALL)
	tar xf $(TARBALL)
	rm -rf $@
	mv $(shell basename $(TARBALL) .tar.xz) $@

$(APP): | $(REPO_PATH)
	cd $(REPO_PATH) && ./configure $(CONFIG_FLAGS)
	$(MAKE) -C $(REPO_PATH)
	cd build && ln -sf ../repo/src/strace strace
