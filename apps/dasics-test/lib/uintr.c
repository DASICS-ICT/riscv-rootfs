#include <stdio.h>
#include <stdlib.h>
#include <machine/syscall.h>

#include "uintr.h"

void prepare_u_intr(void){
    csr_write(0x000,0x11); //set ustatus uie/upie
    csr_write(0x004,0x111); // set uie: enable all u intr
	csr_write(0x005,(uint64_t)u_intr_entry); // set utvec
}


void clear_u_intr(void){
    csr_write(0x000,0x0); // clear ustatus uie/upie
    csr_write(0x004,0x0); // clear uie: disable all u intr
	csr_write(0x005,0x0); // clear utvec
}

void u_intr_handler(void) {
	uint64_t ucause = csr_read(ucause);
    uint64_t utval = csr_read(utval);
    uint64_t uepc = csr_read(uepc);
	printf("[U_INTR_HANDLER] catch u-intr, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx\n", ucause, uepc, utval);
	if (ucause == CAUSE_IRQ_U_TIMER){
		printf("[U_INTR_HANDLER] clear timer\n");
		csr_write(0x045,0);
	}
}