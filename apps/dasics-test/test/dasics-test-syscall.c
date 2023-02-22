#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "udasics.h"

const char *test_info = "[MAIN]-  Test 5: syscall interception test \n";
static char ATTR_ULIB_DATA pub_readonly[100] = "[ULIB]: It's readonly buffer!\n";

unsigned long ATTR_ULIB_TEXT generate_addr() {
	int i = 0xf;
	unsigned long ptr = 0; 
	while (i > 0) {
		ptr |= i;
		ptr <<= 4;
		--i;
	}
	return ptr;
}

int ATTR_ULIB_TEXT ulib_write(int fd, const char* buf, size_t n) {
    int ret_val;
    __asm__ volatile (
        "li	a7, 64\n"
        "mv a0, %[fd]\n"
        "mv a1, %[buf]\n"
        "mv a2, %[n]\n"   
        "ecall\n"  
        "mv %[ret_val], a0"  
        : [ret_val] "=r" (ret_val)
        : [fd] "r" (fd), [buf] "r" (buf), [n] "r" (n)
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

	dasics_umaincall(Umaincall_PRINT, "************* ULIB START ***************** \n", 0, 0); // lib call main 
	char *ptr = (char *)generate_addr();	
	dasics_umaincall(Umaincall_PRINT, "using ecall in lib to write, try to write to stdout\n",0, 0); // lib call main 
    ulib_write(1,"syscall test string 1\n",22);

	dasics_umaincall(Umaincall_PRINT, "using ecall in lib to write, but try to read from the unbounded address: 0x%x, and write to stdout\n", ptr, 0); // lib call main 
    ulib_write(1,ptr,5);// raise fault

	dasics_umaincall(Umaincall_PRINT, "using ecall in lib to write, but try to read from the bounded ready-only address: 0x%x, and write to stdout\n", ptr, 0); // lib call main 
    ulib_write(1,pub_readonly,100);

	dasics_umaincall(Umaincall_PRINT, "using main call to write, but try to read from the unbounded address: 0x%x, and write to stdout\n", ptr, 0); // lib call main 
	dasics_umaincall(Umaincall_WRITE, 1,ptr,5); // lib call main ，should raise fault

	dasics_umaincall(Umaincall_PRINT, "using main call to write, but try to read from the bounded ready-only address: 0x%x, and write to stdout\n", ptr, 0); // lib call main 
	dasics_umaincall(Umaincall_WRITE, 1,pub_readonly,100); // lib call main

	dasics_umaincall(Umaincall_PRINT, "************* ULIB   END ***************** \n", 0, 0); // lib call main 

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

	int32_t idx0;

    idx0 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R                  , (uint64_t)(pub_readonly + 100), (uint64_t)pub_readonly);

	test_syscall();

    dasics_libcfg_free(idx0);

	unregister_udasics();

	return 0;
}