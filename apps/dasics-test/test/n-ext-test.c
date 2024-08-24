#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <machine/syscall.h>

#include "udasics.h"

#define CAUSE_IRQ_U_EXT ((uint64_t)((1ULL<<63) | 8))

const char *test_info = "[MAIN] N-extension u-timer interrupt test\n";

extern void u_int_entry(void);

void exit_function(void) {
	printf("[MAIN] u-timer interrupt test finished\n");
}

void u_int_handler(void) {
	uint64_t ucause = csr_read(ucause);
    uint64_t utval = csr_read(utval);
    uint64_t uepc = csr_read(uepc);
	printf("[U_INT_HANDLER] catch u-int, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx\n", ucause, uepc, utval);
	if (ucause == CAUSE_IRQ_U_EXT){
		printf("[U_INT_HANDLER] clear timer\n");
		csr_write(0x045,0);
	}
}

int main(void) {
	atexit(exit_function);

	printf(test_info);
	
	csr_write(0x045,0);

	for (int i=0;i<10;i++){
		printf("[U_INT_HANDLER] set timer for %d time(s).\n",i);
		csr_write(0x045, 50*1000000); // 1s
		while (csr_read(0x045) != 0);
	}

	exit(0);
	return(0);
}