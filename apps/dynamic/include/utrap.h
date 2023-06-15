#ifndef _DASICS_UTRAP_H_
#define _DASICS_UTRAP_H_

#include <stdio.h>
#include <stdint.h>
#include <asm/dasics_ucontext.h>
#include <udasics.h>

/* Rounding; only works for n = power of two */
#define ROUND(a, n)     (((((uint64_t)(a))+(n)-1)) & ~((n)-1))

#define ROUNDDOWN(a, n) (((uint64_t)(a)) & ~((n)-1))

#define PAGE_SIZE 0X1000


int  trap_libcfg_alloc(regs_context_t* regs, uint64_t cfg, uint64_t hi, uint64_t lo);
int32_t  trap_libcfg_free(regs_context_t* regs, int32_t idx);
uint32_t trap_libcfg_get(regs_context_t* regs, int32_t idx);




#endif