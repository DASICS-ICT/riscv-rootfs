TEST_DIR = $(APP_DIR)/test
LIBS_DIR = $(APP_DIR)/lib

override APP = $(DST_DIR)/dasics-test-free \
	   $(DST_DIR)/dasics-test-jump \
	   $(DST_DIR)/dasics-test-ofb \
	   $(DST_DIR)/dasics-test-syscall \
	   $(DST_DIR)/dasics-test-rwx

include $(RISCV_ROOTFS_HOME)/Makefile.app

ROOTFS_BUILD_DIR = $(RISCV_ROOTFS_HOME)/rootfsimg/build
install: | $(ROOTFS_BUILD_DIR)

$(ROOTFS_BUILD_DIR):
	mkdir -p $@

SRCS = $(TEST_DIR)/dasics-test-free.c \
	   $(TEST_DIR)/dasics-test-jump.c \
	   $(TEST_DIR)/dasics-test-ofb.c \
	   $(TEST_DIR)/dasics-test-rwx.c \
	   $(TEST_DIR)/dasics-test-syscall.c \
	   $(LIBS_DIR)/udasics.c \
	   $(LIBS_DIR)/dasics_entry.S \
	   $(LIBS_DIR)/dasics_call.S

LIBS = lib/udasics.c \
	   lib/dasics_entry.S \
	   lib/dasics_call.S
# cross compile
CROSS_COMPILE = riscv64-unknown-elf-

AS = $(CROSS_COMPILE)gcc
CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
LD = $(CROSS_COMPILE)gcc

# compile option
# Note: gcc -O2 option and RVC will cause 
# some unexpected compilation results.

INCLUDES  = $(addprefix -I, ${INC_DIR})

CFLAGS   += -O0 -march=rv64imad -mabi=lp64d -mno-relax \
	    -msmall-data-limit=0 -MMD $(INCLUDES) -g -static \
	    -DDASICS_S_TRAP_ONLY
CXXFLAGS += -O0 -march=rv64imad -mabi=lp64d -mno-relax \
	      -msmall-data-limit=0 -MMD $(INCLUDES) -g -static \
	      -DDASICS_S_TRAP_ONLY
LDFLAGS  += -march=rv64imad -mabi=lp64d -static -Wl,--no-relax

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

$(APP):%:%.o $(LIBOBJS)
	@echo + LD $@
	@$(LD) $(LDFLAGS) -o $@ $^

# Dependencies
DEPS = $(addprefix $(DST_DIR)/, $(addsuffix .d, $(basename $(SRCS))))
-include $(DEPS)
