#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "udasics.h"

static char ATTR_ULIB_DATA unboundedData[100] 		 = "[ULIB]: It's the unbounded data!";

#pragma GCC optimize("O0")
int ATTR_ULIB_TEXT test_ofb() {

	dasics_umaincall(Umaincall_PRINT, "[INFO] try to load from the unbounded address: 0x%lx\n", unboundedData); // lib call main 
    char data = unboundedData[0]; //should arise uload fault and skip the load instruction
	dasics_umaincall(Umaincall_PRINT, "[INFO] try to store to the unbounded address:  0x%lx\n", unboundedData); // lib call main 
	unboundedData[1] = data;      //should arise ustore fault and skip the store instruction

	return 0;
}


void exit_function() {
	printf("[ERROR] FDI Test 1 FAILED..\n");
}

int main() {

	atexit(exit_function);

	printf("╔═══════════════════════════════════════╗\n");
    printf("║  FDI Test 1: memory out of bound test ║\n");
    printf("╚═══════════════════════════════════════╝\n");

	register_udasics(0);
	lib_call(&test_ofb);
	unregister_udasics();

	return 0;
}
