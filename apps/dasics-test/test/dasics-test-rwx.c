#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "udasics.h"

const char *test_info = "[MAIN]-  Test 3: bound register allocation and authority \n";

static char ATTR_ULIB_DATA secret[100] 		 = "[ULIB1]: It's the secret!";
static char ATTR_ULIB_DATA pub_readonly[100] = "[ULIB1]: It's readonly buffer!";
static char ATTR_ULIB_DATA pub_rwbuffer[100] = "[ULIB1]: It's public rw buffer!";

#pragma GCC optimize("O0")
int ATTR_ULIB_TEXT test_rwx() {
    // Test user main boundarys.
	// Note: gcc -O2 option and RVC will cause 
	// some unexpected compilation results.

	dasics_umaincall(Umaincall_PRINT, "************* ULIB START ***************** \n"); // lib call main 

	dasics_umaincall(Umaincall_PRINT, "try to print the read only buffer: %s\n", pub_readonly);   // That's ok
	dasics_umaincall(Umaincall_PRINT, "try to print the rw buffer: %s\n", pub_rwbuffer); 		 // That's ok

	dasics_umaincall(Umaincall_PRINT, "try to modify the rw buffer: %s\n", pub_rwbuffer);   		 // That's ok
    pub_rwbuffer[19] = pub_readonly[12];  // That's ok
    pub_rwbuffer[21] = 'B';               // That's ok
	dasics_umaincall(Umaincall_PRINT, "new rw buffer: %s\n", pub_rwbuffer);   // That's ok
    
	dasics_umaincall(Umaincall_PRINT, "try to modify read only buffer\n");
	pub_readonly[15] = 'B';               // raise DasicsUStoreAccessFault (0x14)

	dasics_umaincall(Umaincall_PRINT, "try to load from the secret\n");
    char temp = secret[3];                // raise DasicsULoadAccessFault  (0x12)
	dasics_umaincall(Umaincall_PRINT, "try to store to the secret\n");
    secret[3] = temp;                     // raise DasicsUStoreAccessFault (0x14)

	dasics_umaincall(Umaincall_PRINT, "************* ULIB   END ***************** \n"); // lib call main 

	return 0;
}


void exit_function() {
	printf("[MAIN]test dasics finished\n");
}

int main() {
	int32_t idx0, idx1, idx2;

	atexit(exit_function);

	printf(test_info);

	register_udasics(0);

	// Allocate libcfg before calling lib function
    idx0 = dasics_libcfg_alloc(DASICS_LIBCFG_R                  , (uint64_t)pub_readonly, (uint64_t)(pub_readonly + 100));
    idx1 = dasics_libcfg_alloc(DASICS_LIBCFG_R | DASICS_LIBCFG_W, (uint64_t)pub_rwbuffer, (uint64_t)(pub_rwbuffer + 100));
    idx2 = dasics_libcfg_alloc(0                                , (uint64_t)secret      , (uint64_t)(      secret + 100));

	lib_call(&test_rwx);

    // Free those used libcfg via handlers
    dasics_libcfg_free(idx2);
    dasics_libcfg_free(idx1);
    dasics_libcfg_free(idx0);

	unregister_udasics();

	return 0;
}
