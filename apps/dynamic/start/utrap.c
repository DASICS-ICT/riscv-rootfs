#include <utrap.h>
#include <my_stdio.h>

/*
 * This function is used to alloc a couple of bound register
 * from the regs_context_t.
 */
int trap_libcfg_alloc(regs_context_t* regs, uint64_t cfg, uint64_t hi, uint64_t lo)
{
#ifndef RV32
    int32_t step = 8;  // rv64
#else
    int32_t step = 4;  // rv32
#endif
    cfg |= DASICS_LIBCFG_V;
    for (int idx = 0; idx < 16; idx++)
    {
        int choose_libcfg0 = (idx < DASICS_LIBCFG_WIDTH);
        int32_t _idx = choose_libcfg0 ? idx : idx - DASICS_LIBCFG_WIDTH;

        uint64_t *libcfg = choose_libcfg0 ? &(regs->dasicsLibCfg0) :  // DasicsLibCfg0
                                        &(regs->dasicsLibCfg1);  // DasicsLibCfg1

        if (!(((*libcfg) >> (_idx * step)) & DASICS_LIBCFG_V))
        {
            regs->dasicsLibBounds[2 * idx] = hi;
            regs->dasicsLibBounds[2 * idx + 1] = lo;
            (*libcfg) &= ~(0xffUL << (_idx * step));
            (*libcfg) |= (cfg << (_idx * step));
            return idx;
        }   
    }
    return -1;
}


int32_t trap_libcfg_free(regs_context_t* regs, int32_t idx)
{
#ifndef RV32
    int32_t step = 8;  // rv64
#else
    int32_t step = 4;  // rv32
#endif

    int choose_libcfg0 = (idx < DASICS_LIBCFG_WIDTH);
    int32_t _idx = choose_libcfg0 ? idx : idx - DASICS_LIBCFG_WIDTH;

    uint64_t *libcfg = choose_libcfg0 ? &(regs->dasicsLibCfg0) :  // DasicsLibCfg0
                                    &(regs->dasicsLibCfg1);    

    regs->dasicsLibBounds[2 * idx] = 0;
    regs->dasicsLibBounds[2 * idx + 1] = 0;
    (*libcfg) &= ~(0xffUL << (_idx * step));   
}

uint32_t trap_libcfg_get(regs_context_t* regs, int32_t idx)
{
    /* TODO : get the libcfg when trap */
}