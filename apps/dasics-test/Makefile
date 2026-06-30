APP_DIR ?= $(shell pwd)
DST_DIR ?= $(APP_DIR)/build

TEST_DIR = $(APP_DIR)/test
LIBS_DIR = $(APP_DIR)/lib

DASICS_LIB_APP = $(DST_DIR)/dasics-test-free \
	   $(DST_DIR)/dasics-test-jump \
	   $(DST_DIR)/dasics-test-ofb \
	   $(DST_DIR)/dasics-test-syscall \
	   $(DST_DIR)/dasics-test-rwx \
	   $(DST_DIR)/dasics-test-spectre \
	   $(DST_DIR)/n-ext-test \
	   #$(DST_DIR)/mpk
DASICS_STANDALONE_APP = $(DST_DIR)/dasics-test-csr-smoke
override APP = $(DASICS_LIB_APP) $(DASICS_STANDALONE_APP)

include $(RISCV_ROOTFS_HOME)/Makefile.app

SRCS = $(TEST_DIR)/dasics-test-free.c \
	   $(TEST_DIR)/dasics-test-jump.c \
	   $(TEST_DIR)/dasics-test-ofb.c \
	   $(TEST_DIR)/dasics-test-rwx.c \
	   $(TEST_DIR)/dasics-test-syscall.c \
	   $(TEST_DIR)/dasics-test-csr-smoke.c \
	   $(DST_DIR)/dasics-test-spectre.c \
	   $(TEST_DIR)/n-ext-test.c \
	   $(LIBS_DIR)/udasics.c \
	   $(LIBS_DIR)/dasics_entry.S \
	   $(LIBS_DIR)/dasics_call.S \
	   $(LIBS_DIR)/uintr.c \
	   $(LIBS_DIR)/uintr_entry.S \
	   #$(TEST_DIR)/mpk.c 

LIBS = lib/udasics.c \
	   lib/dasics_entry.S \
	   lib/dasics_call.S \
	   lib/uintr.c \
	   lib/uintr_entry.S 
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
STANDALONE_CFLAGS  = -O0 -march=rv64imad -mabi=lp64d -MMD $(INCLUDES) -g -static
STANDALONE_LDFLAGS = -march=rv64imad -mabi=lp64d -static

# Files to be compiled
LIBOBJS = $(addprefix $(DST_DIR)/, $(addsuffix .o, $(basename $(LIBS))))

# Compilation patterns
$(DST_DIR)/lib/%.o: $(LIBS_DIR)/%.c 
	echo + CC $^
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $^
$(DST_DIR)/dasics-test-csr-smoke.o: $(TEST_DIR)/dasics-test-csr-smoke.c
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
