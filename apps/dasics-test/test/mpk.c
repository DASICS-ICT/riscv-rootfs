#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <assert.h>

#include "udasics.h"

void exit_function() {
	printf("[MAIN]test dasics finished\n");
}

int main(void)
{
    atexit(exit_function);
    register_udasics(0);

    printf("[MAIN]-  Test 7: MPK test");

    printf("Set array[4096] to all 'b'\n");
    char __attribute__((aligned(4096))) array[4096] = {'b'};

    printf("Try to change array[4096] to all 'a'\n");
    int i;
    for (i = 0; i < 4096; ++i) array[i] = 'a';  // write ok
    
    int check_a = 1;
    for (i = 0; i < 4096; ++i) if ( array[i] != 'a') check_a = 0;  // read ok
    printf("array = 0x%p, content check %s\n", array, ((check_a)?"success":"fail"));

    // allocate pkey
    printf("Alloc PKEY\n");
    int pkey = pkey_alloc(0, PKEY_DISABLE_WRITE|PKEY_DISABLE_ACCESS);
    printf("pkey = %d\n", pkey);
    assert(pkey > 0);
    int ret = pkey_mprotect(array, sizeof(array), PROT_READ|PROT_WRITE, pkey);
    printf("pkey_mprotect ret = %d\n", ret);
    assert(ret >= 0);

    // trigger faulty access
    printf("Try to Trigger MPK fault per KB\n");
    for (i=0;i<4096;i=i+1024){
    int temp = array[i];  // faulty read
    array[i] = 'n';  // faulty write
    }
    
    unregister_udasics();
    exit(0);

    return 0;
}
