#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "udasics.h"

const char *test_info = "[MAIN]-  Test 1: out of bound test \n";

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

#pragma GCC push_options
#pragma GCC optimize("O0")
int ATTR_ULIB_TEXT test_ofb() {
    // Test user main boundarys.
	// Note: gcc -O2 option and RVC will cause 
	// some unexpected compilation results.

	dasics_umaincall(Umaincall_PRINT, "************* ULIB START ***************** \n", 0, 0); // lib call main 
	char *ptr = (char *)generate_addr();	
	dasics_umaincall(Umaincall_PRINT, "try to load from the unbounded address: 0x%lx\n", ptr, 0); // lib call main 
    char data = *ptr; //should arise uload fault and skip the load instruction
	dasics_umaincall(Umaincall_PRINT, "try to store to the unbounded address:  0x%lx\n", ptr, 0); // lib call main 
	*ptr = data;      //should arise ustore fault and skip the store instruction
	dasics_umaincall(Umaincall_PRINT, "try to jump to the unbounded address:   0x%lx\n", ptr, 0); // lib call main 
	void (*funcptr)() = ptr;
	funcptr();        //should arise ufetch fault and skip the skip instruction
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
	test_ofb();
	unregister_udasics();

	return 0;
}
