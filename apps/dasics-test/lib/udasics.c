#include <stdio.h>
#include <stdlib.h>
#include <machine/syscall.h>
#include "udasics.h"

uint64_t umaincall_helper;

void register_udasics(uint64_t funcptr) 
{
    umaincall_helper = (funcptr != 0) ? funcptr : (uint64_t) dasics_umaincall_helper;
    csr_write(0x8b0, (uint64_t)dasics_umaincall);
    csr_write(0x005, (uint64_t)dasics_ufault_entry);
}

void unregister_udasics(void) 
{
    csr_write(0x8b0, 0);
    csr_write(0x005, 0);    
}

static int bound_coverage_cmp(const void *a, const void *b)
{
    const uint64_t _a = *(const uint64_t *)a;
    const uint64_t _b = *(const uint64_t *)b;
    return (get_dasics_bound_lo(_a) < get_dasics_bound_lo(_b)) ? -1 : 1;
}

static int dasics_bound_checker(uint64_t lo, uint64_t hi, int perm)
{
    // In fact, this is a bound coverage problem for [lo, hi]
    uint64_t bounds[DASICS_MEMCFG_WIDTH];
    int32_t idx, items = 0;
    int32_t max_cfgs = DASICS_MEMCFG_WIDTH;

    // Fill bounds array with permission matched membounds
    for (idx = 0; idx < max_cfgs; ++idx) {
        uint64_t tmp_bound;
        MEM_BOUND_LOOKUP(tmp_bound, idx, READ);

        uint64_t cfg = get_dasics_bound_cfg(tmp_bound);
        if (cfg & DASICS_MEMCFG_V == 0) {
            continue;
        }
        else if ((cfg & (perm | DASICS_MEMCFG_V)) != DASICS_MEMCFG_V) {
            // Permission matched, add this membound to bound list
            bounds[items] = tmp_bound;
            items++;
        }
    }

    // Based on the lower bound, sort bounds array in an increasing order
    qsort(bounds, items, sizeof(uint64_t), bound_coverage_cmp);

    // Calculate bound coverage via greedy algorithm
    for (idx = 0; idx < items; ++idx) {
        uint64_t bound_lo = get_dasics_bound_lo(bounds[idx]);
        uint64_t bound_hi = get_dasics_bound_hi(bounds[idx]);
        if (bound_lo <= lo + 1 && lo <= bound_hi) {
            lo = bound_hi;
        }
        else if (bound_hi < lo) {
            continue;
        }
        else {
            break;
        }
    }

    return hi <= lo;
}

static uint32_t dasics_syscall_checker(SYSCALL_ARGS)
{
    uint32_t retval = 1;

    switch(sysno)
    {
        case SYS_read : case SYS_write :
        case SYS_pread: case SYS_pwrite:
            if (arg3 < 0) {
                // nbytes should not be less than zero
                retval = 0;
            }
            else {
                int perm = (sysno == SYS_read || sysno == SYS_pread) ? DASICS_MEMCFG_W : DASICS_MEMCFG_R;
                retval = dasics_bound_checker((uint64_t)arg2, (uint64_t)arg2 + (uint64_t)arg3 - 1, perm);
            }
            break;
        default:
            break;
    }

    return retval;
}

static long dasics_syscall_proxy(SYSCALL_ARGS)
{
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

uint64_t dasics_umaincall_helper(UmaincallTypes type, ...)
{
    uint64_t dasics_return_pc = csr_read(0x8b1);            // DasicsReturnPC
    uint64_t dasics_free_zone_return_pc = csr_read(0x8b2);  // DasicsFreeZoneReturnPC

    uint64_t retval = 0;

    va_list args;
    va_start(args, type);

    switch (type)
    {
        case Umaincall_PRINT: {
            const char *format = va_arg(args, const char *);
            vprintf(format, args);
        }
        case Umaincall_SETAZONERTPC:
            dasics_free_zone_return_pc = 0x1e9dc;
            break;
        default:
            printf("\x1b[33m%s\x1b[0m","Warning: Invalid umaincall number %d!\n", type); //could not use printf in kernel
            break;
    }

    csr_write(0x8b1, dasics_return_pc);             // DasicsReturnPC
    csr_write(0x8b2, dasics_free_zone_return_pc);   // DasicsFreeZoneReturnPC

    va_end(args);

    return retval;
}

void dasics_ufault_handler(void)
{
    // Save some registers that should be saved by callees
    uint64_t dasics_return_pc = csr_read(0x8b1);
    uint64_t dasics_free_zone_return_pc = csr_read(0x8b2);

    uint64_t ucause = csr_read(ucause);
    uint64_t utval = csr_read(utval);
    uint64_t uepc = csr_read(uepc);
    uint64_t dfreason = csr_read(0x8b3);

    long sysno, arg1, arg2, arg3, arg4, arg5, arg6;
    __asm__ volatile(
        "mv t0, a7\n"
        "mv t1, a0\n"
        "mv t2, a1\n"
        "mv t3, a2\n"
        "mv t4, a3\n"
        "mv t5, a4\n"
        "mv t6, a5\n"
        "sd t0, %[sysno]\n"
        "sd t1, %[arg1]\n"
        "sd t2, %[arg2]\n"
        "sd t3, %[arg3]\n"
        "sd t4, %[arg4]\n"
        "sd t5, %[arg5]\n"
        "sd t6, %[arg6]\n"
        : [arg1] "=m" (arg1), [arg2] "=m" (arg2), [arg3] "=m" (arg3), \
          [arg4] "=m" (arg4), [arg5] "=m" (arg5), [arg6] "=m" (arg6), \
          [sysno] "=m" (sysno)
        :: "memory"
    );
    // here only handle DasicsUCheckFault
    switch(dfreason)
    {
        case EXC_DASICS_JUMP_FAULT:
            printf("[DASICS UEXCEPTION]Info: dasics jump fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx, dfreason = 0x%lx\n", ucause, uepc, utval, dfreason);
            break;
        case EXC_DASICS_LOAD_FAULT:
            printf("[DASICS UEXCEPTION]Info: dasics load fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx, dfreason = 0x%lx\n", ucause, uepc, utval, dfreason);
            break;
        case EXC_DASICS_STORE_FAULT:
            printf("[DASICS UEXCEPTION]Info: dasics store fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx, dfreason = 0x%lx\n", ucause, uepc, utval, dfreason);
            break;
        case EXC_DASICS_ECALL_FAULT:
            //printf("[DASICS EXCEPTION]Info: dasics uecall fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx\n", ucause, uepc, utval);
            printf("[DASICS UEXCEPTION]Info: dasics lib ecall occurs (ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx, dfreason = 0x%lx), try to check arguments...\n", ucause, uepc, utval, dfreason);
            if(dasics_syscall_checker(sysno, arg1, arg2, arg3, arg4, arg5, arg6)){
                    printf("[DASICS UEXCEPTION]Info: lib ecall arguments OK! sycall number:%d, syscall is permitted \n", sysno);
                    uint64_t ret = dasics_syscall_proxy(sysno, arg1, arg2, arg3, arg4, arg5, arg6);
                    csr_write(uepc, uepc + 4);         
                    csr_write(0x8b1, dasics_return_pc);
                    csr_write(0x8b2, dasics_free_zone_return_pc);
                    return;
            } 
            printf("\x1b[31m%s\x1b[0m","[DASICS UEXCEPTION]Error: lib ecall arguments beyond authority, dasics ecall fault occurs!\n");
            break;
        case EXC_MPK_LOAD_FAULT:
            printf("[DASICS UEXCEPTION]Info: mpk load fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx, dfreason = 0x%lx\n", ucause, uepc, utval, dfreason);
            break;
        case EXC_MPK_STORE_FAULT:
            printf("[DASICS UEXCEPTION]Info: mpk store fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx, dfreason = 0x%lx\n", ucause, uepc, utval, dfreason);
            break;
        default:
            printf("\x1b[31m%s\x1b[0m","[DASICS UEXCEPTION]Error: unexpected dasics fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx, dfreason = 0x%lx\n", ucause, uepc, utval, dfreason);
            break;
    }
    printf("[DASICS UEXCEPTION]Info: dasics_return_pc:0x%lx\n", dasics_return_pc);	

    // switch base on cause types.
    // currently just skip this inst.
    csr_write(uepc, uepc + 4);
    // rvc will compress jump/branch inst.
    //if (ucause == EXC_DASICS_UFETCH_FAULT)
      //  csr_write(uepc, uepc + 2);
    //else 
       // csr_write(uepc, uepc + 4); 

    // Restore those saved registers
    csr_write(0x8b1, dasics_return_pc);
    csr_write(0x8b2, dasics_free_zone_return_pc);

}

int32_t dasics_membound_alloc(uint64_t cfg, uint64_t lo, uint64_t hi) {
    int32_t max_cfgs = DASICS_MEMCFG_WIDTH;

    for (int32_t idx = 0; idx < max_cfgs; ++idx) {
        uint64_t tmp_bound;
        MEM_BOUND_LOOKUP(tmp_bound,idx,READ);
        uint64_t curr_cfg = get_dasics_bound_cfg(tmp_bound);

        if ((curr_cfg & DASICS_MEMCFG_V) == 0)  // Found available config
        {
            // Write DASICS bounds csr
            tmp_bound = cal_dasics_bound_val(lo,hi,((cfg & DASICS_MEMCFG_MASK) | DASICS_MEMCFG_V));
            MEM_BOUND_LOOKUP(tmp_bound,idx,WRITE);
            return idx;
        }
    }

    return -1;
}

uint64_t dasics_membound_get(int32_t idx) {
    uint64_t val;
    if (idx < 0 || idx >= DASICS_MEMCFG_WIDTH) return -1;
    MEM_BOUND_LOOKUP(val,idx,READ);
    return val;
}

int32_t dasics_membound_set(int32_t idx, uint64_t val) {
    if (idx < 0 || idx >= DASICS_MEMCFG_WIDTH) return -1;
    MEM_BOUND_LOOKUP(val,idx,WRITE);
    return 0;
}

int32_t dasics_jmpbound_alloc(uint64_t lo, uint64_t hi) {
    int32_t max_cfgs = DASICS_JMPCFG_WIDTH;
    for (int32_t idx = 0; idx < max_cfgs; ++idx) {
        uint64_t tmp_bound;
        JMP_BOUND_LOOKUP(tmp_bound,idx,READ);
        uint64_t curr_cfg = get_dasics_bound_cfg(tmp_bound);
        if ((curr_cfg & DASICS_JMPCFG_V) == 0) // found available cfg
        {
            // Write DASICS bounds csr
            tmp_bound = cal_dasics_bound_val(lo,hi,DASICS_JMPCFG_V);
            MEM_BOUND_LOOKUP(tmp_bound,idx,WRITE);
            return idx;
        }
    }

    return -1;
}

uint64_t dasics_jmpbound_get(int32_t idx) {
    uint64_t val;
    if (idx < 0 || idx >= DASICS_JMPCFG_WIDTH) return -1;
    JMP_BOUND_LOOKUP(val,idx,READ);
    return val;
}

int32_t dasics_jmpbound_set(int32_t idx, uint64_t val) {
    if (idx < 0 || idx >= DASICS_JMPCFG_WIDTH) return -1;
    JMP_BOUND_LOOKUP(val,idx,WRITE);
    return 0;
}
