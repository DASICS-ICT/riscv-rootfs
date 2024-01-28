NAME = busybox

include $(RISCV_ROOTFS_HOME)/Makefile.app

REPO_PATH = repo
$(REPO_PATH):
	mkdir -p $@
	git clone --depth 1 -b 1_36_stable git://git.busybox.net/busybox $@
	cp config $@/.config

$(APP): | $(REPO_PATH)
	CROSS_COMPILE=riscv64-unknown-linux-gnu- $(MAKE) -C $(REPO_PATH)
	ln -sf $(abspath $(REPO_PATH)/busybox) $@
