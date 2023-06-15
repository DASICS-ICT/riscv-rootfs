#ifndef _INCLUDE_CROSS_H
#define _INCLUDE_CROSS_H

#include <asm/dasics_ucontext.h>
#include <stdint.h>
#include <stddef.h>

/* MAX depth of cross call */
#define MAX_DEPTH 4096

extern uint64_t jurisdiction_stack;

typedef struct cross_call
{
    umain_got_t * entry;
    umain_got_t * target;
	reg_t ra;
	/* dasics user registers */
	reg_t dasicsLibCfg0;
	reg_t dasicsLibCfg1;
	reg_t dasicsLibBounds[32];

	reg_t dasicsMaincallEntry;
	reg_t dasicsReturnPC;
	reg_t dasicsFreeZoneReturnPC;
} cross_call_t;

void init_libcfg_stack();

/* push cgf and pop cfg */
cross_call_t * push_libcfg(umain_got_t * entry, umain_got_t * target, regs_context_t * regs);
cross_call_t * pop_libcfg(regs_context_t * regs);







#endif