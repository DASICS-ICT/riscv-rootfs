#define _GNU_SOURCE
#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "udasics.h"

#define TEST_LOOP 1000
#define BUFFER_SIZE 4 * 1024 * 1024 //4M
#define read_csr(reg) ({ unsigned long __tmp; \
  asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
  __tmp; })

#define write_csr(reg, val) ({ \
  asm volatile ("csrw " #reg ", %0" :: "rK"(val)); })

#define rdcycle() read_csr(cycle)
#define rdtime() read_csr(time)

char __attribute__((aligned(4096))) array[BUFFER_SIZE] = {'b'};
char __attribute__((aligned(4096))) temp_array[BUFFER_SIZE] = {'b'};

void test_mpk(int* buffer, uint64_t size) {
    uint64_t start = rdcycle();

    for(int i =0; i < TEST_LOOP; i++){
        // Key Allocation (R)
        int buffer_pkey = pkey_alloc(0, PKEY_DISABLE_WRITE);
        // Bind allocated pkeys with arrays
        int ret = pkey_mprotect(buffer, size, PROT_READ, buffer_pkey);
        // Release allocated pkeys
        int pkey_na =pkey_free(buffer_pkey);
    }

    uint64_t end = rdcycle();

    printf("[MPK-%d] INFO: total time_elapsed: %lu cycles\n", size, end - start);    
}

void test_dasics(int* buffer, uint64_t size) {
    uint64_t start = rdcycle();

    for(int i =0; i < TEST_LOOP; i++){
        //Bound Allocation (R)
        int idx0 = dasics_libcfg_alloc(DASICS_LIBCFG_R, (uint64_t)buffer, (uint64_t)(buffer + size));
        // Release allocated pkeys
        dasics_libcfg_free(idx0);
    }

    uint64_t end = rdcycle();

    printf("[DASICS-%d] INFO: total time_elapsed: %lu cycles\n", size, end - start);    
}

void test_memcpy(int* buffer, uint64_t size) {
    uint64_t start = rdcycle();

    for(int i =0; i < TEST_LOOP; i++){
        memcpy(temp_array, buffer, size);
    }

    uint64_t end = rdcycle();

    printf("[MEMCPY-%d] INFO: total time_elapsed: %lu cycles\n", size, end - start);    
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <size>\n", argv[0]);
        return 1;
    }

    int size = atoi(argv[1]);

    if (size <= 0 || size > BUFFER_SIZE) {
        printf("Invalid size: %d\n", size);
        return 1;
    }

    test_memcpy(array,size);
    test_dasics(array, size);
    test_mpk(array, size);

    return 0;
}