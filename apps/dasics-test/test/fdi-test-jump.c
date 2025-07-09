#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "udasics.h"


int ATTR_UFREEZONE_TEXT in_bound_func()
{
	dasics_umaincall(Umaincall_PRINT, "[INFO] this function is in jump bound...\n");

	dasics_umaincall(Umaincall_PRINT, "[INFO] try to jump to an out-of-bound function...\n"); 
	out_bound_func(); 

	dasics_umaincall(Umaincall_PRINT, "[ERROR] should not execute to here !!!!\n"); 
	return 0;
}

int ATTR_ULIB_TEXT test_jump() {

	dasics_umaincall(Umaincall_PRINT, "[INFO] try to jump to an in-bound function...\n"); 
	in_bound_func(); 

	return 0;
}


#pragma GCC push_options
#pragma GCC optimize("O0")
int out_bound_func() {

	printf("[ERROR] should not jump to here !!!!\n"); 

	return 0;
}


#pragma GCC pop_options
void exit_function() {
	printf("[ERROR] FDI Test 2 FAILED..\n");
}

int main() {
	atexit(exit_function);

    printf("╔═══════════════════════════════════════╗\n");
    printf("║   FDI Test 2: jump out of bound test  ║\n");
    printf("╚═══════════════════════════════════════╝\n");

	register_udasics(0);

	lib_call(&test_jump);

	unregister_udasics();

	return 0;
}
