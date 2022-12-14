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

#pragma GCC push_options
#pragma GCC optimize("O0")
int ATTR_ULIB_TEXT test_rwx() {
    // Test user main boundarys.
	// Note: gcc -O2 option and RVC will cause 
	// some unexpected compilation results.

	dasics_umaincall(Umaincall_PRINT, "************* ULIB START ***************** \n", 0, 0); // lib call main 

	dasics_umaincall(Umaincall_PRINT, "try to print the read only buffer: %s\n", pub_readonly, 0);   // That's ok
	dasics_umaincall(Umaincall_PRINT, "try to print the rw buffer: %s\n", pub_rwbuffer, 0); 		 // That's ok

	dasics_umaincall(Umaincall_PRINT, "try to modify the rw buffer: %s\n", pub_rwbuffer, 0);   		 // That's ok
    pub_rwbuffer[19] = pub_readonly[12];  // That's ok
    pub_rwbuffer[21] = 'B';               // That's ok
	dasics_umaincall(Umaincall_PRINT, "new rw buffer: %s\n", pub_rwbuffer, 0);   // That's ok
    
	dasics_umaincall(Umaincall_PRINT, "try to modify read only buffer\n", 0, 0);
	pub_readonly[15] = 'B';               // raise DasicsUStoreAccessFault (0x14)

	dasics_umaincall(Umaincall_PRINT, "try to load from the secret\n", 0, 0);
    char temp = secret[3];                // raise DasicsULoadAccessFault  (0x12)
	dasics_umaincall(Umaincall_PRINT, "try to store to the secret\n", 0, 0);
    secret[3] = temp;                     // raise DasicsUStoreAccessFault (0x14)
	dasics_umaincall(Umaincall_PRINT, "try to jump to the secret\n", 0, 0);
    void (*funcptr)() = secret;           
	funcptr();                            // raise DasicsUInstrAccessFault (0x14)

	dasics_umaincall(Umaincall_PRINT, "************* ULIB   END ***************** \n", 0, 0); // lib call main 

	return 0;
}

#pragma GCC pop_options

void exit_function() {
	printf("[MAIN]test dasics finished\n");
}

int main() {
	int32_t idx0, idx1, idx2;

	atexit(exit_function);

	printf(test_info);

	register_udasics(0);

	// Allocate libcfg before calling lib function
    idx0 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R                  , (uint64_t)(pub_readonly + 100), (uint64_t)pub_readonly);
    idx1 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R | DASICS_LIBCFG_W, (uint64_t)(pub_rwbuffer + 100), (uint64_t)pub_rwbuffer);
    idx2 = dasics_libcfg_alloc(DASICS_LIBCFG_V                                    , (uint64_t)(      secret + 100), (uint64_t)secret);

	test_rwx();

    // Free those used libcfg via handlers
    dasics_libcfg_free(idx2);
    dasics_libcfg_free(idx1);
    dasics_libcfg_free(idx0);

	unregister_udasics();

	return 0;
}
