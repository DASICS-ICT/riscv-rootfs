#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "udasics.h"

void ATTR_ULIB_TEXT rop_target()
{
    printf("success.\nROP function reached.\n");
    exit(0);
}

int ATTR_UFREEZONE_TEXT vunerable_func()
{
    char stack_buffer[32];
	dasics_umaincall(Umaincall_PRINT, "[INFO] return address will be changed..\n");

    ((uint64_t* )stack_buffer)[(32 + 8) / 8] = (uint64_t)&rop_target + 8;

	return 0;
}

int ATTR_ULIB_TEXT test_jump() {
    // Test user main boundarys.
	// Note: gcc -O2 option and RVC will cause 
	// some unexpected compilation results.

	dasics_umaincall(Umaincall_PRINT, "[INFO] try to jump to an vunerable function...\n"); 
	vunerable_func(); 

	return 0;
}

#pragma GCC pop_options
void exit_function() {
	printf("[ERROR] FDI Test 3 FAILED..\n");
}

int main() {
	atexit(exit_function);

    printf("╔═══════════════════════════════════════╗\n");
    printf("║  FDI Test 3: control flow check test  ║\n");
    printf("╚═══════════════════════════════════════╝\n");

	register_udasics(0);

	lib_call(&test_jump);

	unregister_udasics();

	return 0;
}
