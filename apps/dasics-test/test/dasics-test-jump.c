#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "udasics.h"

const char *test_info = "[MAIN]-  Test 2: lib function jump or call \n";

#pragma GCC push_options
#pragma GCC optimize("O0")
int test_jump_main() {
    // Test user main boundarys.
	// Note: gcc -O2 option and RVC will cause 
	// some unexpected compilation results.

	printf("[UMAIN]should not jump to here !!!!\n"); 

	return 0;
}

int ATTR_UFREEZONE_TEXT test_jump_lib() {
    // Test user main boundarys.
	// Note: gcc -O2 option and RVC will cause 
	// some unexpected compilation results.

	printf("[ULIB]should not jump to here !!!!\n"); 

	return 0;
}

int ATTR_ULIB_TEXT test_free_to_lib() {
    // Test user main boundarys.
	// Note: gcc -O2 option and RVC will cause 
	// some unexpected compilation results.

	dasics_umaincall(Umaincall_PRINT, "[ULIB] should not jump to here !!!!\n"); 

	return 0;
}

int ATTR_UFREEZONE_TEXT test_normal_free()
{
	dasics_umaincall(Umaincall_PRINT, "This is a normal free call\n");

	return 0;
}

int ATTR_UFREEZONE_TEXT test_jump() {
    // Test user main boundarys.
	// Note: gcc -O2 option and RVC will cause 
	// some unexpected compilation results.

	dasics_umaincall(Umaincall_PRINT, "************* ULIB START ***************** \n"); // lib call main 

	dasics_umaincall(Umaincall_PRINT, "try to printf directly without maincall\n"); // lib call main 
    printf("should not print this info"); //raise fault

	dasics_umaincall(Umaincall_PRINT, "try to jump to main function\n"); // lib call main 
    test_jump_main(); //raise fault

	dasics_umaincall(Umaincall_PRINT, "try to jump from freezone area to lib area\n"); // free to lib
	test_free_to_lib(); // raise fault

	dasics_umaincall(Umaincall_PRINT, "try to jump between freezone area\n"); // free to free
	test_normal_free(); // ok

	dasics_umaincall(Umaincall_PRINT, "************* ULIB   END ***************** \n"); // lib call main 

	return 0;
}

int ATTR_UFREEZONE_TEXT test_normal_lib()
{
	dasics_umaincall(Umaincall_PRINT, "************* ULIB START ***************** \n"); // lib call main 

	dasics_umaincall(Umaincall_PRINT, "This is a normal lib call\n");

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

	// main -> lib -> return normal
	lib_call(&test_normal_lib);

	// 
	lib_call(&test_jump);


	lib_call(&test_jump_lib);

	unregister_udasics();

	return 0;
}
