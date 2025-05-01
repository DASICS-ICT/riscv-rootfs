#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <syscall.h>
#include <time.h>
#include <sys/mman.h>
#include <stdint.h>

#include "udasics.h"

#define TRAIN_LOOP  500
#define TEST_LOOP 1000

#define read_csr(reg) ({ unsigned long __tmp; \
  asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
  __tmp; })

#define rdcycle() read_csr(cycle)
#define rdtime() read_csr(time)

#define BOUND_REG_READ(idx)   \
            csr_read(0x890 + idx * 2);  \
            csr_read(0x891 + idx * 2);  \

#define BOUND_REG_WRITE(hi,lo,idx)   \
            csr_write(0x890 + idx * 2, lo);  \
            csr_write(0x891 + idx * 2, hi);  \

#define SYSCALL_ARGS  long sysno, long arg1, long arg2, \
        long arg3, long arg4, long arg5, long arg6

static inline long __attribute__((always_inline)) ulib_syscall(SYSCALL_ARGS) {
    register long a0 asm("a0") = arg1;
    register long a1 asm("a1") = arg2;
    register long a2 asm("a2") = arg3;
    register long a3 asm("a3") = arg4;
    register long a4 asm("a4") = arg5;
    register long a5 asm("a5") = arg6;
    register long a7 asm("a7") = sysno;

    asm volatile("ecall"                        \
                 : "+r"(a0)                     \
                 : "r"(a1), "r"(a2), "r"(a3),   \
                   "r"(a4), "r"(a5), "r"(a7)    \
                 : "memory");

    return a0;
}

#define ULIB_SYSCALL6(sysno, arg1, arg2, arg3, arg4, arg5, arg6) \
	ulib_syscall(sysno, (long)arg1, (long)arg2, (long)arg3, (long)arg4, (long)arg5, (long)arg6)
#define ULIB_SYSCALL4(sysno, arg1, arg2, arg3, arg4) \
	ULIB_SYSCALL6(sysno, arg1, arg2, arg3, arg4, 0, 0)
#define ULIB_SYSCALL3(sysno, arg1, arg2, arg3) \
	ULIB_SYSCALL6(sysno, arg1, arg2, arg3, 0, 0, 0)
#define ULIB_SYSCALL1(sysno, arg1) \
	ULIB_SYSCALL6(sysno, arg1, 0, 0, 0, 0, 0)

//syscall in trusted region
#pragma GCC optimize("O0")
static inline void tfunc_syscall(){
	ULIB_SYSCALL1(SYS_getuid,0);
	return;
}

//syscall in untrusted region
#pragma GCC optimize("O0")
static inline void ATTR_ULIB_TEXT ufunc_syscall(){
	ULIB_SYSCALL1(SYS_getuid,0);
	return;
}

#pragma GCC optimize("O0")
void uoptest_dasicscall_jr(){
    uint64_t start = rdcycle();

    for(int i = 0; i < TEST_LOOP; i++){
         __asm__ volatile (
            ".word 0x0005108b\n"    // dasicscall.jr ra, a0, jump to the address in a0 (nop instruction)
            "nop\n"                 // Next instruction: nop
            :                       // No output operands
            :                       // No input operands
            : "a0"                  // Inform the compiler that a0 is modified
        );
    } 

    uint64_t end = rdcycle();

    printf("[ INFO ] bound csr write cpu cycles:\t %lu cycles\n",  (end - start)/TEST_LOOP);   
}

#pragma GCC optimize("O0")
void uoptest_bound_rw(){
    uint64_t start = rdcycle();

    for(int i = 0; i < TEST_LOOP; i++){
        BOUND_REG_WRITE(0, 0, 0);
    } 

    uint64_t end = rdcycle();

    printf("[ INFO ] bound write cpu cycles:\t %lu cycles\n",  (end - start)/TEST_LOOP);   

    start = rdcycle();

    for(int i = 0; i < TEST_LOOP; i++){
        BOUND_REG_READ(0);
    } 

    end = rdcycle();

    printf("[ INFO ] bound read cpu cycles:\t %lu cycles\n",  (end - start)/TEST_LOOP);   
}

#pragma GCC optimize("O0")
void uoptest_syscall_interception(){
    uint64_t start = rdcycle();

    for(int i = 0; i < TEST_LOOP; i++){
        lib_call(&ufunc_syscall);
    } 

    uint64_t end = rdcycle();

	uint64_t ufunc_syscall_time = (end - start)/TEST_LOOP;


    start = rdcycle();

    for(int i = 0; i < TEST_LOOP; i++){
        tfunc_syscall();
    } 

    end = rdcycle();

	uint64_t tfunc_syscall_time = (end - start)/TEST_LOOP;

    printf("[ INFO ] syscall interception cpu cycles:\t %lu cycles\n",  ufunc_syscall_time - tfunc_syscall_time);   
}


#pragma GCC optimize("O0")
int main(){
	register_udasics(0);	

	printf("------------------------------------------\n");
	uoptest_bound_rw();
	printf("------------------------------------------\n");
	uoptest_syscall_interception();
	printf("------------------------------------------\n");
	uoptest_dasicscall_jr();
	
    unregister_udasics();
	return 0;
}