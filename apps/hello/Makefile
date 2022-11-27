NAME = hello
INCLUDES += ./include
CROSS_COMPILE = riscv64-unknown-elf-
SRCS = hello.c dasics/udasics.c dasics/dasics_entry.S #crt0.S
LDS = $(NAME).lds

CFLAGS += -T $(LDS) -O0 
# LDFLAGS += -nostartfiles 

include $(RISCV_ROOTFS_HOME)/Makefile.compile


