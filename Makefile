$(shell mkdir -p rootfsimg/build)

APPS = hello
APPS_DIR = $(addprefix apps/, $(APPS))
DASICS_LINUX_BUILD_DIR ?= $(CURDIR)/rootfsimg/build/opensbi-linux-dasics
DASICS_LINUX_CC ?= riscv64-unknown-linux-gnu-gcc
DASICS_LINUX_STRIP ?= riscv64-unknown-linux-gnu-strip
DASICS_LINUX_TEST_NAMES = \
	dasics-test-csr-smoke \
	dasics-test-csr-mask-ops-smoke \
	dasics-test-call-jr-returnpc-smoke \
	dasics-test-maincfg-toggle-smoke \
	dasics-test-jump-branch-target-permission-smoke \
	dasics-test-load-store-permission-smoke \
	dasics-test-ecall-fault-smoke \
	dasics-test-ecall-close-smoke \
	dasics-test-syscall-buffer-permission-smoke \
	dasics-test-dynamic-bound-free-smoke \
	dasics-test-dynamic-bound-replacement-smoke \
	dasics-test-context-section-load-smoke \
	dasics-test-context-switch-isolation-smoke \
	dasics-test-returnpc-trap-preserve-smoke \
	dasics-test-rwx \
	dasics-test-jump \
	dasics-test-ofb \
	dasics-test-free \
	dasics-test-syscall
DASICS_LINUX_TEST_BINS = \
	$(addprefix $(DASICS_LINUX_BUILD_DIR)/tests/,$(DASICS_LINUX_TEST_NAMES))
DASICS_LINUX_MAINCFG_BIN = \
	$(DASICS_LINUX_BUILD_DIR)/tests/dasics-test-maincfg-toggle-smoke
DASICS_LINUX_STRIP_ALL_BINS = \
	$(filter-out $(DASICS_LINUX_MAINCFG_BIN),$(DASICS_LINUX_TEST_BINS))

.PHONY: rootfsimg $(APPS_DIR) opensbi-linux-dasics clean

all: $(APPS_DIR)

rootfsimg: $(APPS_DIR)

opensbi-linux-dasics:
	mkdir -p $(DASICS_LINUX_BUILD_DIR)/tests
	$(MAKE) -C apps/dasics-test \
		RISCV_ROOTFS_HOME=$(CURDIR) \
		DST_DIR=$(DASICS_LINUX_BUILD_DIR)/tests \
		DASICS_LINUX_DUAL_EXEC=1 \
		build-only
	$(DASICS_LINUX_CC) -O2 -static -march=rv64imad -mabi=lp64d \
		-Wall -Wextra -Werror \
		-o $(DASICS_LINUX_BUILD_DIR)/init \
		rootfsimg/init-opensbi-linux-dasics.c
	$(DASICS_LINUX_STRIP) --strip-all \
		$(DASICS_LINUX_BUILD_DIR)/init \
		$(DASICS_LINUX_STRIP_ALL_BINS)
	$(DASICS_LINUX_STRIP) --strip-debug $(DASICS_LINUX_MAINCFG_BIN)

$(APPS_DIR): %:
	-$(MAKE) -s -C $@ install

clean:
	-$(foreach app, $(APPS_DIR), $(MAKE) -s -C $(app) clean ;)
	-rm -f rootfsimg/build/*
