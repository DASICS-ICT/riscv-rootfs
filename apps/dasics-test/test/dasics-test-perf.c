#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <machine/syscall.h>

#include "udasics.h"

#define TRAIN_TIME 100
#define TEST_TIME 20

// const char *test_info = "[MAIN]-  Test 6: dasics operation time perf test (using Verilator printf) \n";

static uint64_t umain_val = 0xdeadbeef;
static uint64_t ATTR_ULIB_DATA ulib_val = 0xdeadbeef;

#pragma GCC optimize("O0")
void test_umain_jump(){
	return;
}
#pragma GCC optimize("O0")
static inline void test_umain_ldst(){
	asm volatile (
		"nop"
		: [buf] "+r" (umain_val)
		:
		: "memory"
	);
	return;
}

#pragma GCC optimize("O0")
static inline void test_umain_syscall(){
	ULIB_SYSCALL1(SYS_getuid,0);
	return;
}

#pragma GCC optimize("O0")
static inline void ATTR_ULIB_TEXT test_ulib_jump(){
	return;
}

#pragma GCC optimize("O0")
static inline void ATTR_ULIB_TEXT test_ulib_ldst(){
	asm volatile (
		"nop"
		: [buf] "+r" (ulib_val)
		:
		: "memory"
	);
	return;
}

#pragma GCC optimize("O0")
static inline void ATTR_ULIB_TEXT test_ulib_syscall(){
	ULIB_SYSCALL1(SYS_getuid,0);
	return;
}

// void exit_function() {
// 	printf("[MAIN] test dasics finished\n");
// }

#pragma GCC optimize("O0")
int main(){
	int i;
    // atexit(exit_function);
	// printf(test_info);
	register_udasics_direct(0);	
    //TODO: TEST CONTENT
	// printf("set dasics env\n");
	int32_t idx0 = dasics_libcfg_alloc(DASICS_LIBCFG_R | DASICS_LIBCFG_W, (uint64_t)&ulib_val, (uint64_t)(&ulib_val + 8));
	// printf("train btb\n");
	for (i=0;i<TRAIN_TIME;i++) test_umain_jump();


	// printf("dummy write csrw\n");
	for (i=0;i<TEST_TIME;i++) {
		asm volatile ("nop;":::);
	}
	// printf("write dasics csrw\n");
	for (i=0;i<TEST_TIME;i++) csr_write(0x8b2,0);

	// printf("no_check jump\n");
	for (i=0;i<TEST_TIME;i++) main_call(&test_umain_jump);
		// printf("check jump\n");
	for (i=0;i<TEST_TIME;i++) lib_call(&test_ulib_jump);

	// printf("no_check ldst\n");
	for (i=0;i<TEST_TIME;i++) lib_call(&test_umain_ldst);
	// printf("check ldst\n");
	for (i=0;i<TEST_TIME;i++) lib_call(&test_ulib_ldst);
	// printf("no_check syscall\n");
	for (i=0;i<TEST_TIME;i++) lib_call(&test_umain_syscall);
	// printf("check syscall\n");
	for (i=0;i<TEST_TIME;i++) lib_call(&test_ulib_syscall);

	dasics_libcfg_free(idx0);
    unregister_udasics();
	return 0;
}