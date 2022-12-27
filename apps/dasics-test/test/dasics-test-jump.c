#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "udasics.h"

const char *test_info = "[MAIN]-  Test 2: lib function jump or call \n";

static char ATTR_ULIB_DATA secret[100] 		 = "[ULIB1]: It's the secret!";
static char ATTR_ULIB_DATA pub_readonly[100] = "[ULIB1]: It's readonly buffer!";
static char ATTR_ULIB_DATA pub_rwbuffer[100] = "[ULIB1]: It's public rw buffer!";

#pragma GCC push_options
#pragma GCC optimize("O0")
int test_jump_main() {
    // Test user main boundarys.
	// Note: gcc -O2 option and RVC will cause 
	// some unexpected compilation results.

	printf("[UMAIN]should not jump to here !!!!\n"); 

	return 0;
}

#pragma GCC push_options
#pragma GCC optimize("O0")
int ATTR_ULIB_TEXT test_jump_lib() {
    // Test user main boundarys.
	// Note: gcc -O2 option and RVC will cause 
	// some unexpected compilation results.

	printf("[ULIB]should not jump to here !!!!\n"); 

	return 0;
}

 #pragma GCC push_options
 #pragma GCC optimize("O0")
 int ATTR_UFREEZONE_TEXT test_free_zone() {
    // Test user main boundarys.
 	// Note: gcc -O2 option and RVC will cause 
 	// some unexpected compilation results
	dasics_umaincall(Umaincall_PRINT, " - - - - - - UFREEZONE START - - - - - - - -  \n", 0, 0); // lib call main 

	dasics_umaincall(Umaincall_PRINT, "try to jump to lib function \n", 0, 0); // lib call main 
	test_jump_lib(); //raise fault

	dasics_umaincall(Umaincall_PRINT, " - - - - - - UFREEZONE  END - - - - - - - -   \n", 0, 0); // lib call main 
	return 0;
 }

#pragma GCC push_options
#pragma GCC optimize("O0")
int ATTR_ULIB_TEXT test_jump() {
    // Test user main boundarys.
	// Note: gcc -O2 option and RVC will cause 
	// some unexpected compilation results.

	dasics_umaincall(Umaincall_PRINT, "************* ULIB START ***************** \n", 0, 0); // lib call main 

	dasics_umaincall(Umaincall_PRINT, "try to printf directly without maincall\n", 0, 0); // lib call main 
    printf("should not print this info"); //raise fault

	dasics_umaincall(Umaincall_PRINT, "try to jump to main function\n", 0, 0); // lib call main 
    test_jump_main(); //raise fault

	dasics_umaincall(Umaincall_PRINT, "try to jump to freezone function\n", 0, 0); // lib call main 
    test_free_zone();

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

	test_jump();

	unregister_udasics();

	return 0;
}
