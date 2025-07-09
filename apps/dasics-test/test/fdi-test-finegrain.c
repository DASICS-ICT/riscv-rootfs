#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "udasics.h"

static char ATTR_ULIB_DATA unboundedData_8_bytes[8] 	= "ABCDEFGH";

#pragma GCC optimize("O0")
int ATTR_ULIB_TEXT test_ofb() {
	dasics_umaincall(Umaincall_PRINT, "[INFO] try to store from the 8 bytes bounded buffer...\n"); // lib call main 
    unboundedData_8_bytes[7] = 'B';  

	return 0;
}


void exit_function() {
	printf("[ERROR] FDI Test 5 FAILED..\n");
}

int main() {

	atexit(exit_function);
    printf("╔═══════════════════════════════════════╗\n");
    printf("║ FDI Test 5:fine-grain protection test ║\n");
    printf("╚═══════════════════════════════════════╝\n");

	register_udasics(0);
	int32_t idx0 = dasics_libcfg_alloc(DASICS_LIBCFG_R  , (uint64_t)unboundedData_8_bytes, (uint64_t)8);

	lib_call(&test_ofb);

    dasics_libcfg_free(idx0);
	unregister_udasics();

	return 0;
}
