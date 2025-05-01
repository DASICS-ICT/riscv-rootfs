NAME = cf2025-api
SRCS = $(NAME).c

DASICS_SUPPORT = $(abspath $(RISCV_ROOTFS_HOME)/LibDASICS)
INC_DIR += $(DASICS_SUPPORT)/include
LDFLAGS += -L$(DASICS_SUPPORT)/build -lDASICS -T$(DASICS_SUPPORT)/ld.lds

ifneq ($(RISCV_ROOTFS_HOME),)
include $(RISCV_ROOTFS_HOME)/Makefile.compile
else
$(error RISCV_ROOTFS_HOME is not defined)
endif