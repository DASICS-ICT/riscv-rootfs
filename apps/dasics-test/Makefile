NAME = dasics-test
DST_DIR ?= ./build
TEST_DIR = ./test
LIBS_DIR = ./lib

TEST = $(DST_DIR)/dasics-test-free \
	   $(DST_DIR)/dasics-test-jump \
	   $(DST_DIR)/dasics-test-ofb \
	   $(DST_DIR)/dasics-test-rwx 
	   #$(DST_DIR)/dasics-test-syscall

SRCS = $(TEST_DIR)/dasics-test-free.c \
	   $(TEST_DIR)/dasics-test-jump.c \
	   $(TEST_DIR)/dasics-test-ofb.c \
	   $(TEST_DIR)/dasics-test-rwx.c \
	   $(TEST_DIR)/dasics-test-syscall.c \
	   lib/udasics.c \
	   lib/dasics_entry.S

LIBS = lib/udasics.c lib/dasics_entry.S
# cross compile

CROSS_COMPILE_PREFIX = ~/tools/llvm-project/build/bin/
CROSS_COMPILE = $(CROSS_COMPILE_PREFIX)clang

AS = $(CROSS_COMPILE)
CC = $(CROSS_COMPILE)
CXX = $(CROSS_COMPILE)++
LD = $(CROSS_COMPILE)

# compile option
# Note: gcc -O2 option and RVC will cause 
# some unexpected compilation results.

LDS = $(NAME).lds
INC_DIR += ./include/

INCLUDES  = $(addprefix -I, ${INC_DIR})

CFLAGS   += --sysroot=/data/jinyue/riscv-toolchain/dasics/dasics-riscv-toolchain/riscv64-unknown-elf/riscv64-unknown-elf --gcc-toolchain=/data/jinyue/riscv-toolchain/dasics/dasics-riscv-toolchain/riscv64-unknown-elf -O0 -march=rv64gc -MMD $(INCLUDES) -g  -T $(LDS)

CXXFLAGS += -O0 -march=rv64imad -MMD $(INCLUDES) -g  -T $(LDS)

LDFLAGS  += --sysroot=/data/jinyue/riscv-toolchain/dasics/dasics-riscv-toolchain/riscv64-unknown-elf/riscv64-unknown-elf --gcc-toolchain=/data/jinyue/riscv-toolchain/dasics/dasics-riscv-toolchain/riscv64-unknown-elf -O0 -march=rv64gc

# Files to be compiled
LIBOBJS = $(addprefix $(DST_DIR)/, $(addsuffix .o, $(basename $(LIBS))))

# Compilation patterns
$(DST_DIR)/lib/%.o: $(LIBS_DIR)/%.c 
	echo + CC $^
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $^
$(DST_DIR)/%.o: $(TEST_DIR)/%.c 
	echo + CC $^
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $^
$(DST_DIR)/lib/%.o: $(LIBS_DIR)/%.S   
	echo + CC $^
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $^

$(TEST):%:%.o $(LIBOBJS)
	@echo + LD $@
	@$(LD) $(LDFLAGS) -o $@ $^

.PHONY: all clean

include $(RISCV_ROOTFS_HOME)/Makefile.app

TEST_LIST = dasics-test-free dasics-test-jump dasics-test-ofb dasics-test-rwx 

all:$(TEST)
	-$(foreach test, $(TEST_LIST), ln -sf $(abspath .)/build/$(test) $(RISCV_ROOTFS_HOME)/rootfsimg/build/$(test);)

clean: 
	rm -rf $(DST_DIR)

# Dependencies
DEPS = $(addprefix $(DST_DIR)/, $(addsuffix .d, $(basename $(SRCS))))
-include $(DEPS)


