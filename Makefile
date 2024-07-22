CROSS_COMPILE = riscv64-unknown-linux-gnu-
INITRAMFS_ROOT := $(shell pwd)/initramfs

export RISCV
export CROSS_COMPILE
export INITRAMFS_ROOT


INITRAMFS_TXT := initramfs.txt

APPS := libtirpc busybox lmbench
APPS-CLEAN := $(foreach app,$(APPS),$(app)-clean)

.PHONY: all clean $(APPS) $(APPS-CLEAN)

all: $(INITRAMFS_TXT)

$(INITRAMFS_ROOT):
	mkdir -p $(INITRAMFS_ROOT)

$(INITRAMFS_TXT): $(APPS)
	./scripts/gen-initramfs-list.sh $@

lmbench: libtirpc

clean: $(APPS-CLEAN)
	rm -rf $(INITRAMFS_ROOT) $(INITRAMFS_TXT)

$(APPS): %: $(INITRAMFS_ROOT)
	$(MAKE) -C apps/scripts/$* all

$(APPS-CLEAN): %-clean:
	$(MAKE) -C apps/scripts/$* clean
