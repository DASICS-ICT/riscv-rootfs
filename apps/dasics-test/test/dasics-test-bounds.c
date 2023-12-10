#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include "udasics.h"

const char *test_info = "[MAIN]-  Test 6: test dasics bounds allocation \n";

static char ATTR_ULIB_DATA pub_readonly[160] = "[ULIB6] Info: It's public rw buffer! ";
static char ATTR_ULIB_DATA pub_rwbuffer[160] = "[ULIB6] Error: rwbuffer data is old! ";

#pragma GCC push_options
#pragma GCC optimize("O0")
int ATTR_ULIB_TEXT test_bounds() {
    // Test user main boundarys.
	// Note: gcc -O2 option and RVC will cause 
	// some unexpected compilation results.

    for (int i = 0; i < sizeof(pub_rwbuffer) / sizeof(char); ++i) {
        pub_rwbuffer[i] = '\0';
    }

    for (int i = 0; i < sizeof(pub_readonly) / sizeof(char); ++i) {
        if (pub_readonly[i] != '\0') {
            pub_rwbuffer[i] = pub_readonly[i];
        } else {
            break;
        }
    }

	return 0;
}

#pragma GCC pop_options

void exit_function() {
	printf("[MAIN]test dasics finished\n");
}

int main() {
	int32_t handles_rw[16];
	int32_t handles_ro[16];

	atexit(exit_function);

	printf(test_info);

	register_udasics(0);

    uint64_t range_len = 10;
    uint64_t baseaddr = (uint64_t)pub_rwbuffer;
    for (int i = 0; i < sizeof(handles_rw) / sizeof(int32_t); ++i) {
        handles_rw[i] = dasics_libcfg_alloc(DASICS_LIBCFG_R | DASICS_LIBCFG_W, baseaddr, baseaddr + range_len - 1);
        assert(handles_rw[i] >= 0);
        baseaddr += range_len;
    }

    dasics_print_cfg_register(handles_rw[29]);

    baseaddr = (uint64_t)pub_readonly;
    for (int i = 0; i < sizeof(handles_ro) / sizeof(int32_t); ++i) {
        handles_ro[i] = dasics_libcfg_alloc(DASICS_LIBCFG_R, baseaddr, baseaddr + range_len - 1);
        assert(handles_ro[i] >= 0);
        baseaddr += range_len;
    }

    dasics_print_cfg_register(handles_ro[19]);

    lib_call(&test_bounds);

    for (int i = 0; i < sizeof(handles_rw) / sizeof(int32_t); ++i) {
        assert(dasics_libcfg_free(handles_rw[i]) == 0);
    }

    for (int i = 0; i < sizeof(handles_ro) / sizeof(int32_t); ++i) {
        assert(dasics_libcfg_free(handles_ro[i]) == 0);
    }

    printf("%s\n", pub_rwbuffer);

	unregister_udasics();

	return 0;
}
