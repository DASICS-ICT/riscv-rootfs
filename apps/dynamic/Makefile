# ------------------------------------------------------------
# DIR build
# ------------------------------------------------------------
DIR_BUILD 		= ./build
DIR_START		= ./start
DIR_MLIB		= ./main_lib
DIR_MEM 		= ./memory
DIR_ECALL		= ./ecall

# ------------------------------------------------------------
# the target file name
# ------------------------------------------------------------
NAME 			= dynamic
READ_FILE 		= cert.crt

# ------------------------------------------------------------
# the dynamic file name
# ------------------------------------------------------------
MY_LIB 			= mylib.so
MY_LIB1 		= mylib1.so

# ------------------------------------------------------------
# the compile logic
# ------------------------------------------------------------
# CROSS_COMPILE = riscv64-unknown-linux-gnu-

CROSS_COMPILE = /home/wanghan/riscv/riscv64-unknown-linux-gnu-nod/bin/riscv64-unknown-linux-gnu-

CFLAGS = -O2 -g -Iinclude -Itest_lib_header -Idasics_dll/include -Imemory/include -Iecall/include -w -DDASICS_LINUX -DDASICS_COPY #-DDASICS_DEBUG

CC = $(CROSS_COMPILE)gcc
OBJDUMP = $(CROSS_COMPILE)objdump
HEXDUMP = hexdump

# DASICS entry point
DASICS_LINKER 	= ld.lds 
USER_ENTRYPOINT = 0x1000000


# ------------------------------------------------------------
# DASICS test file, include library
# ------------------------------------------------------------
SRC_TEST   		= $(wildcard ./test/*.c)
SRC_MY_LIB		= ./test_lib/mylib.c 
SRC_MY_LIB1		= ./test_lib/mylib1.c 


# ------------------------------------------------------------
# DASICS main logic file
# ------------------------------------------------------------
SRC_ENTRY  		= $(DIR_START)/entry.S
SRC_DAISCS 		= $(wildcard ./dasics/*.c) $(wildcard ./dasics/*.S) 
SRC_DASICS_URAP = $(wildcard ./start/*.c)
SRC_DASICS_MEM  = $(wildcard ./memory/*.c)
SRC_DASICS_ECALL= $(wildcard ./ecall/*.c)

SRC_DASICS_LINK	= $(wildcard ./dasics_dll/*.c)
SRCS 			= $(SRC_ENTRY) \
				  $(SRC_DAISCS) \
				  $(SRC_DASICS_LINK) \
				  $(SRC_DASICS_URAP) \
				  $(SRC_TEST) \
				  $(SRC_DASICS_MEM) \
				  $(SRC_DASICS_ECALL)

# ------------------------------------------------------------
# DASICS inline my_printf, equal printf logically
# ------------------------------------------------------------
SRC_MY_PRINTF 	= $(DIR_MLIB)/my_printf.c
ELF_MY_PRINTF	= $(DIR_BUILD)/my_printf.o


# main logic
all: dirs lib lib1 my_printf elf

dirs:
	@mkdir -p $(DIR_BUILD)

lib:
	$(CC) $(CFLAGS) -fPIC -shared -o $(MY_LIB) $(SRC_MY_LIB) 

lib1: $(MY_LIB) lib
	$(CC) $(CFLAGS) -fno-builtin -nostdlib -fPIC -shared -o $(MY_LIB1) $(SRC_MY_LIB1) $(MY_LIB)
my_printf:
	$(CC) $(CFLAGS) -fno-builtin -nostdlib -c $(SRC_MY_PRINTF) -o $(ELF_MY_PRINTF)

elf: $(MY_LIB) $(MY_LIB1) $(ELF_MY_PRINTF) lib lib1 my_printf
	$(CC) $(CFLAGS) -o  $(NAME) $(SRCS) $(MY_LIB) $(MY_LIB1) $(ELF_MY_PRINTF) -e _set_utvec -T $(DASICS_LINKER)
	
	@cp $(MY_LIB) $(DIR_BUILD)
	@cp $(MY_LIB1) $(DIR_BUILD)
	@cp $(NAME) $(DIR_BUILD)
	@cp $(READ_FILE) $(DIR_BUILD)
	@rm $(MY_LIB) $(MY_LIB1) $(NAME)
	@$(OBJDUMP) -d $(DIR_BUILD)/$(NAME) > ./build/dynamic.txt
	@$(OBJDUMP) -d $(DIR_BUILD)/$(MY_LIB) > ./build/mylib.txt
	@$(OBJDUMP) -d $(DIR_BUILD)/$(MY_LIB1) > ./build/mylib1.txt
	@$(HEXDUMP) -C $(DIR_BUILD)/$(NAME) > ./build/dynamic_.txt
	@$(HEXDUMP) -C $(DIR_BUILD)/$(MY_LIB) > ./build/mylib_.txt
	@$(HEXDUMP) -C $(DIR_BUILD)/$(MY_LIB1) > ./build/mylib1_.txt

clean:
	rm -rf build

install:
	ln -sf $(RISCV_ROOTFS_HOME)/apps/dynamic/build/dynamic $(RISCV_ROOTFS_HOME)/rootfsimg/build/dasics-test-dynamic
	ln -sf $(RISCV_ROOTFS_HOME)/apps/dynamic/build/mylib.so $(RISCV_ROOTFS_HOME)/rootfsimg/build/mylib.so
	ln -sf $(RISCV_ROOTFS_HOME)/apps/dynamic/build/mylib1.so $(RISCV_ROOTFS_HOME)/rootfsimg/build/mylib1.so
	ln -sf $(RISCV_ROOTFS_HOME)/apps/dynamic/build/cert.crt $(RISCV_ROOTFS_HOME)/rootfsimg/build/cert.crt
