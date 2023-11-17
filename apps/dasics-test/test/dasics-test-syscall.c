#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <machine/syscall.h>

#include "udasics.h"

const char *test_info = "[MAIN]-  Test 5: syscall interception test \n";
static char ATTR_ULIB_DATA pub_readonly[100] = "[ULIB]: It's readonly buffer!\n";
static char ATTR_ULIB_DATA covered_fully1[100] = "[ULIB]: This buffer is fully covered by DASICS libbounds!\n";
static char ATTR_ULIB_DATA covered_fully2[100] = "[ULIB]: That buffer is fully covered by DASICS libbounds!\n";
static char ATTR_ULIB_DATA covered_partially[100] = "[ULIB] ERROR: This buffer is partially covered, and should not be printed!\n";

static inline int __attribute__((always_inline)) ulib_write(int fd, const char* buf, size_t n) {
    int ret_val;
    __asm__ volatile (
        "li	a7, %[sysno]\n"
        "mv a0, %[fd]\n"
        "mv a1, %[buf]\n"
        "mv a2, %[n]\n"   
        "ecall\n"  
        "mv %[ret_val], a0"  
        : [ret_val] "=r" (ret_val)
        : [fd] "r" (fd), [buf] "r" (buf), [n] "r" (n), [sysno] "i" (SYS_write)
        : "a7", "a0", "a1", "a2", "memory"
    );
    return ret_val;
}


#pragma GCC push_options
#pragma GCC optimize("O0")
int ATTR_ULIB_TEXT test_syscall() {
    // Test user main boundarys.
	// Note: gcc -O2 option and RVC will cause 
	// some unexpected compilation results.
	int retval = 0;

	dasics_umaincall(Umaincall_PRINT, "************* ULIB START ***************** \n"); // lib call main 
	char *ptr = (char *)0xffffffffabcdef00;
	dasics_umaincall(Umaincall_PRINT, "using ecall in lib to write, try to write to stdout\n"); // lib call main 
    retval = ulib_write(1,"syscall test string 1\n",22);
	dasics_umaincall(Umaincall_PRINT, "write retval = %d\n", retval);

	dasics_umaincall(Umaincall_PRINT, "using ecall in lib to write, but try to read from the unbounded address: 0x%lx, and write to stdout\n", ptr); // lib call main 
    retval = ulib_write(1,ptr,5);// raise fault
	dasics_umaincall(Umaincall_PRINT, "write retval = %d\n", retval);

	dasics_umaincall(Umaincall_PRINT, "using ecall in lib to write, but try to read from the bounded ready-only address: 0x%lx, and write to stdout\n", pub_readonly); // lib call main 
    retval = ulib_write(1,pub_readonly,100);
	dasics_umaincall(Umaincall_PRINT, "write retval = %d\n", retval);

	dasics_umaincall(Umaincall_PRINT, "Test umaincall_print va_list: %d, %d, %d, %d, %d\n", 1, 2, 3, 4, 5);

	dasics_umaincall(Umaincall_PRINT, "Try to write the 1st fully covered buffer to stdout\n");
	retval = ulib_write(1, covered_fully1, 100);
	dasics_umaincall(Umaincall_PRINT, "write retval = %d\n", retval);

	dasics_umaincall(Umaincall_PRINT, "Try to write the 2nd fully covered buffer to stdout\n");
	retval = ulib_write(1, covered_fully2, 100);
	dasics_umaincall(Umaincall_PRINT, "write retval = %d\n", retval);

	dasics_umaincall(Umaincall_PRINT, "Try to write the partially covered buffer to stdout\n");
	retval = ulib_write(1, covered_partially, 100);
	dasics_umaincall(Umaincall_PRINT, "write retval = %d\n", retval);

	dasics_umaincall(Umaincall_PRINT, "************* ULIB   END ***************** \n"); // lib call main 

	return 0;
}

#pragma GCC pop_options

void exit_function() {
	printf("[MAIN]test dasics finished\n");
}

int main() {

	atexit(exit_function);

	printf(test_info);

	register_udasics(0);

	int32_t idx0 = dasics_libcfg_alloc(DASICS_LIBCFG_R, (uint64_t)pub_readonly,  (uint64_t)(pub_readonly + 99));

	// For fully covered buffer 1st
	int32_t idx1 = dasics_libcfg_alloc(DASICS_LIBCFG_R, (uint64_t)(covered_fully1     ), (uint64_t)(covered_fully1 + 10));
	int32_t idx2 = dasics_libcfg_alloc(DASICS_LIBCFG_R, (uint64_t)(covered_fully1 +  9), (uint64_t)(covered_fully1 + 80));
	int32_t idx3 = dasics_libcfg_alloc(DASICS_LIBCFG_R, (uint64_t)(covered_fully1 + 81), (uint64_t)(covered_fully1 + 99));

	// For partially covered buffer
	int32_t idx4 = dasics_libcfg_alloc(DASICS_LIBCFG_R, (uint64_t)(covered_partially     ), (uint64_t)(covered_partially + 10));
	int32_t idx5 = dasics_libcfg_alloc(DASICS_LIBCFG_R, (uint64_t)(covered_partially + 81), (uint64_t)(covered_partially + 99));

	// For fully covered buffer 2nd
	int32_t idx6 = dasics_libcfg_alloc(DASICS_LIBCFG_R, (uint64_t)(covered_fully2 -  1), (uint64_t)(covered_fully2     ));
	int32_t idx7 = dasics_libcfg_alloc(DASICS_LIBCFG_R, (uint64_t)(covered_fully2 +  1), (uint64_t)(covered_fully2 + 99));

	lib_call(&test_syscall);

    dasics_libcfg_free(idx0);
	dasics_libcfg_free(idx1);
	dasics_libcfg_free(idx2);
	dasics_libcfg_free(idx3);
	dasics_libcfg_free(idx4);
	dasics_libcfg_free(idx5);
	dasics_libcfg_free(idx6);
	dasics_libcfg_free(idx7);

	unregister_udasics();
	exit(0);

	return 0;
}
