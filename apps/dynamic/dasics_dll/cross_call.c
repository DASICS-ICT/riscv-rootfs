#include <dasics_link.h>
#include <stddef.h>
#include <utrap.h>
#include <dasics_link_manager.h>
#include <lib-names.h>
#include <stdlib.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>
#include <cross.h>

// libcfg stack top 
uint64_t jurisdiction_stack = 0UL;
static uint64_t jurisdiction_stack_base;
static int limit = 16;

/* This function is used to init libcfg stack */
void init_libcfg_stack()
{
    /* align the heap for 128bit */
    jurisdiction_stack = __BRK(ROUND(__BRK(NULL), 0x16UL));    

    jurisdiction_stack_base = jurisdiction_stack;

    jurisdiction_stack += sizeof(cross_call_t) * MAX_DEPTH;
    /* MAX DEPTH NUM */
    __BRK(jurisdiction_stack);

}

/* push libcfg in the jurisdiction_stack */
cross_call_t * push_libcfg(umain_got_t * entry, umain_got_t * target, regs_context_t * regs)
{
    if (jurisdiction_stack <= jurisdiction_stack_base)
    {
        my_printf("[ERROR] Cross lib call too much!\n");
        /* let exit(1) could be executed normally */
        exit(1);
    }

    cross_call_t * now = (cross_call_t *)(jurisdiction_stack - sizeof(cross_call_t));
    now->entry = entry;
    now->target = target;
    jurisdiction_stack -= sizeof(cross_call_t);
    
    now->ra = regs->ra;
    // we can used memcpy which in trusted libc.so.6
    reg_t * reg_i = &(regs->dasicsLibCfg0);
    reg_t * reg_j = &(now->dasicsLibCfg0);
    for (int _i = 0; _i < sizeof(cross_call_t) / sizeof(reg_t) - 3; _i++)
    {
        reg_j[_i] = reg_i[_i];
    }
    // memcpy(reg_j, reg_i, sizeof(cross_call_t) - 3 * sizeof(reg_t));
    return now;
}

/* pop libcfg from the jurisdiction_stack */
cross_call_t * pop_libcfg(regs_context_t * regs)
{
    cross_call_t * now = (cross_call_t *)(jurisdiction_stack);

    jurisdiction_stack += sizeof(cross_call_t);

    regs->ra = now->ra;
    // we can used memcpy which in trusted libc.so.6
    reg_t * reg_i = &(now->dasicsLibCfg0);
    reg_t * reg_j = &(regs->dasicsLibCfg0);
    for (int _i = 0; _i < sizeof(cross_call_t) / sizeof(reg_t) - 3; _i++)
    {
        reg_j[_i] = reg_i[_i];
    }    
    // memcpy(reg_j, reg_i, sizeof(cross_call_t) - 3 * sizeof(reg_t));

    return now;
}