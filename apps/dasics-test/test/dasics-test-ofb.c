#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "udasics.h"

const char *test_info = "[MAIN]-  Test 1: out of bound test \n";

static char ATTR_ULIB_DATA unboundedData[100] 		 = "[ULIB]: It's the unbounded data!";

#pragma GCC optimize("O0")
int ATTR_ULIB_TEXT test_ofb() {
    // Test user main boundarys.
	// Note: gcc -O2 option and RVC will cause 
	// some unexpected compilation results.

	dasics_umaincall(Umaincall_PRINT, "************* ULIB START ***************** \n", 0, 0); // lib call main 
	dasics_umaincall(Umaincall_PRINT, "try to load from the unbounded address: 0x%lx\n", unboundedData, 0); // lib call main 
    char data = unboundedData[0]; //should arise uload fault and skip the load instruction
	dasics_umaincall(Umaincall_PRINT, "try to store to the unbounded address:  0x%lx\n", unboundedData, 0); // lib call main 
	unboundedData[1] = data;      //should arise ustore fault and skip the store instruction
	dasics_umaincall(Umaincall_PRINT, "************* ULIB   END ***************** \n", 0, 0); // lib call main 

	return 0;
}


void exit_function() {
	printf("[MAIN]test dasics finished\n");
}

int main() {

	atexit(exit_function);

	printf(test_info);

	register_udasics(0);
	lib_call(&test_ofb);
	unregister_udasics();

	return 0;
}
