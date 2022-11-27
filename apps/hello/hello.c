#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "udasics.h"

const char *hello = "[Hello] Hello, RISC-V world!\n";

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
int ATTR_ULIB_TEXT test_dasics() {

    // Test user main boundarys.
	// Note: gcc -O2 option and RVC will cause 
	// some unexpected compilation results.
	char *ptr = (char *)generate_addr();	

	char data = *ptr; // uload fault  	
	dasics_umaincall(Umaincall_PRINT, "Try to get secret data\n", 0, 0); // lib call main 
	*ptr = data;  // ustore fault
	void (*funcptr)() = ptr;
	funcptr();// ufetch fault

	return 0;
}

void test_switch() {
	/* Test thread switch context */
	#define PAGE_SIZE 4096 
	int array[4 * PAGE_SIZE];

	for (int i = 0; i < PAGE_SIZE; ++i) {
		for (int j = 0; j < 4; ++j) {
			array[i+PAGE_SIZE*j] = i + j;
		}
	}

}
#pragma GCC pop_options

void exit_function() {
	printf("test dasics finished\n");
}

int main() {

	atexit(exit_function);

	printf(hello);

	//test_switch();

	register_udasics(0);
	test_dasics();
	unregister_udasics();

	return 0;
}
