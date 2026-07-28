APP_DIR ?= $(shell pwd)
DST_DIR ?= $(APP_DIR)/build

TEST_DIR = $(APP_DIR)/test
LIBS_DIR = $(APP_DIR)/lib

DASICS_LIB_APP = $(DST_DIR)/dasics-test-free \
	   $(DST_DIR)/dasics-test-jump \
	   $(DST_DIR)/dasics-test-ofb \
	   $(DST_DIR)/dasics-test-syscall \
	   $(DST_DIR)/dasics-test-rwx \
	   $(DST_DIR)/dasics-test-spectre
DASICS_STANDALONE_APP = $(DST_DIR)/dasics-test-csr-smoke \
	   $(DST_DIR)/dasics-test-csr-mask-ops-smoke \
	   $(DST_DIR)/dasics-test-call-jr-returnpc-smoke \
	   $(DST_DIR)/dasics-test-jump-branch-target-permission-smoke \
	   $(DST_DIR)/dasics-test-load-store-permission-smoke \
	   $(DST_DIR)/dasics-test-ecall-fault-smoke \
	   $(DST_DIR)/dasics-test-ecall-close-smoke \
	   $(DST_DIR)/dasics-test-dynamic-bound-free-smoke \
	   $(DST_DIR)/dasics-test-dynamic-bound-replacement-smoke \
	   $(DST_DIR)/dasics-test-syscall-buffer-permission-smoke \
	   $(DST_DIR)/dasics-test-context-section-load-smoke \
	   $(DST_DIR)/dasics-test-context-switch-isolation-smoke \
	   $(DST_DIR)/dasics-test-returnpc-trap-preserve-smoke \
	   $(DST_DIR)/dasics-test-maincfg-toggle-smoke
override APP = $(DASICS_LIB_APP) $(DASICS_STANDALONE_APP)

include $(RISCV_ROOTFS_HOME)/Makefile.app

SRCS = $(TEST_DIR)/dasics-test-free.c \
	   $(TEST_DIR)/dasics-test-jump.c \
	   $(TEST_DIR)/dasics-test-ofb.c \
	   $(TEST_DIR)/dasics-test-rwx.c \
	   $(TEST_DIR)/dasics-test-syscall.c \
	   $(TEST_DIR)/dasics-test-csr-smoke.c \
	   $(TEST_DIR)/dasics-test-csr-mask-ops-smoke.c \
	   $(TEST_DIR)/dasics-test-call-jr-returnpc-smoke.c \
	   $(TEST_DIR)/dasics-test-jump-branch-target-permission-smoke.c \
	   $(TEST_DIR)/dasics-test-load-store-permission-smoke.c \
	   $(TEST_DIR)/dasics-test-ecall-fault-smoke.c \
	   $(TEST_DIR)/dasics-test-ecall-close-smoke.c \
	   $(TEST_DIR)/dasics-test-dynamic-bound-free-smoke.c \
	   $(TEST_DIR)/dasics-test-dynamic-bound-replacement-smoke.c \
	   $(TEST_DIR)/dasics-test-syscall-buffer-permission-smoke.c \
	   $(TEST_DIR)/dasics-test-context-section-load-smoke.c \
	   $(TEST_DIR)/dasics-test-context-switch-isolation-smoke.c \
	   $(TEST_DIR)/dasics-test-returnpc-trap-preserve-smoke.c \
	   $(TEST_DIR)/dasics-test-maincfg-toggle-smoke.c \
	   $(DST_DIR)/dasics-test-spectre.c \
	   $(LIBS_DIR)/udasics.c \
	   $(LIBS_DIR)/dasics_entry.S \
	   $(LIBS_DIR)/dasics_call.S

LIBS = lib/udasics.c \
	   lib/dasics_entry.S \
	   lib/dasics_call.S
# cross compile
CROSS_COMPILE = riscv64-unknown-elf-
DASICS_STANDALONE_CROSS_COMPILE ?= riscv64-unknown-linux-gnu-

AS = $(CROSS_COMPILE)gcc
CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
LD = $(CROSS_COMPILE)gcc
STANDALONE_CC = $(DASICS_STANDALONE_CROSS_COMPILE)gcc
STANDALONE_LD = $(DASICS_STANDALONE_CROSS_COMPILE)gcc

# compile option
# Note: gcc -O2 option and RVC will cause 
# some unexpected compilation results.

INCLUDES  = $(addprefix -I, ${INC_DIR})

CFLAGS   += -O0 -march=rv64imad -mabi=lp64d -MMD $(INCLUDES) -g -static
CXXFLAGS += -O0 -march=rv64imad -mabi=lp64d -MMD $(INCLUDES) -g -static
LDFLAGS  += -march=rv64imad -mabi=lp64d -static
DASICS_S_TRAP_ONLY ?= 1
DASICS_N_EXTENSION_PROFILE ?= 0
ifeq ($(filter 0 1,$(strip $(DASICS_S_TRAP_ONLY))),)
$(error DASICS_S_TRAP_ONLY must be 0 or 1)
endif
ifeq ($(filter 0 1,$(strip $(DASICS_N_EXTENSION_PROFILE))),)
$(error DASICS_N_EXTENSION_PROFILE must be 0 or 1)
endif
ifeq ($(strip $(DASICS_N_EXTENSION_PROFILE)),1)
ifneq ($(strip $(DASICS_S_TRAP_ONLY)),0)
$(error DASICS_N_EXTENSION_PROFILE requires DASICS_S_TRAP_ONLY=0)
endif
CFLAGS   += -DDASICS_N_EXTENSION_PROFILE
endif
ifeq ($(strip $(DASICS_S_TRAP_ONLY)),1)
# The legacy standalone profile delegates DASICS faults to UCAS OS S-mode.
CFLAGS   += -DDASICS_S_TRAP_ONLY
endif
STANDALONE_CFLAGS  = -O0 -march=rv64imad -mabi=lp64d -mno-relax -msmall-data-limit=0 -MMD $(INCLUDES) -g -static
STANDALONE_LDFLAGS = -march=rv64imad -mabi=lp64d -static -Wl,--no-relax

# Files to be compiled
LIBOBJS = $(addprefix $(DST_DIR)/, $(addsuffix .o, $(basename $(LIBS))))

# Compilation patterns
$(DST_DIR)/lib/%.o: $(LIBS_DIR)/%.c 
	echo + CC $^
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $^
$(DST_DIR)/dasics-test-csr-smoke.o $(DST_DIR)/dasics-test-csr-mask-ops-smoke.o $(DST_DIR)/dasics-test-call-jr-returnpc-smoke.o $(DST_DIR)/dasics-test-jump-branch-target-permission-smoke.o $(DST_DIR)/dasics-test-load-store-permission-smoke.o $(DST_DIR)/dasics-test-ecall-fault-smoke.o $(DST_DIR)/dasics-test-ecall-close-smoke.o $(DST_DIR)/dasics-test-dynamic-bound-free-smoke.o $(DST_DIR)/dasics-test-dynamic-bound-replacement-smoke.o $(DST_DIR)/dasics-test-syscall-buffer-permission-smoke.o $(DST_DIR)/dasics-test-context-section-load-smoke.o $(DST_DIR)/dasics-test-context-switch-isolation-smoke.o $(DST_DIR)/dasics-test-returnpc-trap-preserve-smoke.o $(DST_DIR)/dasics-test-maincfg-toggle-smoke.o: $(DST_DIR)/%.o: $(TEST_DIR)/%.c
	echo + CC $^
	mkdir -p $(dir $@)
	$(STANDALONE_CC) $(STANDALONE_CFLAGS) -c -o $@ $^
$(DST_DIR)/%.o: $(TEST_DIR)/%.c 
	echo + CC $^
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $^
$(DST_DIR)/lib/%.o: $(LIBS_DIR)/%.S   
	echo + CC $^
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $^

$(DASICS_LIB_APP):%:%.o $(LIBOBJS)
	@echo + LD $@
	@$(LD) $(LDFLAGS) -o $@ $^

$(DASICS_STANDALONE_APP):%:%.o
	@echo + LD $@
	@$(STANDALONE_LD) $(STANDALONE_LDFLAGS) -o $@ $^

.PHONY: install-csr-smoke

install-csr-smoke: $(DASICS_STANDALONE_APP)
	ln -sf $< $(RISCV_ROOTFS_HOME)/rootfsimg/build/$(notdir $<)

# Dependencies
DEPS = $(addprefix $(DST_DIR)/, $(addsuffix .d, $(basename $(SRCS))))
-include $(DEPS)
