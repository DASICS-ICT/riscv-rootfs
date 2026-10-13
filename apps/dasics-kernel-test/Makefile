LINUX_SRC ?=
LINUX_BUILD_DIR ?=
BUILD_DIR ?= $(CURDIR)/build
CROSS_COMPILE ?= riscv64-linux-gnu-
JOBS ?= 24
SUITES := data-bound-refill fault-recovery maincall-service
KERNEL_MAKE = $(MAKE) -C "$(abspath $(LINUX_SRC))" \
	O="$(abspath $(LINUX_BUILD_DIR))" ARCH=riscv CROSS_COMPILE="$(CROSS_COMPILE)"

.PHONY: all check-kernel kernel-modules $(SUITES)
all: $(SUITES)

check-kernel:
	@test -n "$(LINUX_SRC)" || { echo 'LINUX_SRC is required'; exit 1; }
	@test -n "$(LINUX_BUILD_DIR)" || { echo 'LINUX_BUILD_DIR is required'; exit 1; }
	@grep -qx 'CONFIG_RISCV_DASICS_KERNEL=y' "$(LINUX_BUILD_DIR)/.config" || \
		{ echo 'The kernel must enable CONFIG_RISCV_DASICS_KERNEL'; exit 1; }
	@grep -qx 'CONFIG_MODULES=y' "$(LINUX_BUILD_DIR)/.config" || \
		{ echo 'The kernel must enable CONFIG_MODULES'; exit 1; }

kernel-modules: check-kernel
	$(KERNEL_MAKE) -j$(JOBS) modules

$(SUITES): kernel-modules
	mkdir -p "$(BUILD_DIR)/$@-utmod" "$(BUILD_DIR)/$@-tmod"
	cp -a "$@-utmod/." "$(BUILD_DIR)/$@-utmod/"
	cp -a "$@-tmod/." "$(BUILD_DIR)/$@-tmod/"
	$(KERNEL_MAKE) M="$(abspath $(BUILD_DIR))/$@-utmod" -j$(JOBS) modules
	$(KERNEL_MAKE) M="$(abspath $(BUILD_DIR))/$@-tmod" \
		KBUILD_EXTRA_SYMBOLS="$(abspath $(BUILD_DIR))/$@-utmod/Module.symvers" \
		-j$(JOBS) modules
