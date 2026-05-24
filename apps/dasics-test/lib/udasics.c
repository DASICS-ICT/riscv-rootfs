#include <stdio.h>
#include <stdlib.h>
#include <machine/syscall.h>
#include "udasics.h"

uint64_t umaincall_helper;
static uint64_t ufault_print_info = 1;

#define BOUND_REG_READ(hi,lo,idx)   \
        case idx:  \
            lo = csr_read(0x890 + idx * 2);  \
            hi = csr_read(0x891 + idx * 2);  \
            break;

#define BOUND_REG_WRITE(hi,lo,idx)   \
        case idx:  \
            DASICS_PROLOGUE(); \
            csr_write(0x890 + idx * 2, lo);  \
            csr_write(0x891 + idx * 2, hi);  \
            break;

#define CONCAT(OP) BOUND_REG_##OP

#define LIBBOUND_LOOKUP(HI,LO,IDX,OP) \
        switch (IDX) \
        {               \
            CONCAT(OP)(HI,LO,0);  \
            CONCAT(OP)(HI,LO,1);  \
            CONCAT(OP)(HI,LO,2);  \
            CONCAT(OP)(HI,LO,3);  \
            CONCAT(OP)(HI,LO,4);  \
            CONCAT(OP)(HI,LO,5);  \
            CONCAT(OP)(HI,LO,6);  \
            CONCAT(OP)(HI,LO,7);  \
            CONCAT(OP)(HI,LO,8);  \
            CONCAT(OP)(HI,LO,9);  \
            CONCAT(OP)(HI,LO,10); \
            CONCAT(OP)(HI,LO,11); \
            CONCAT(OP)(HI,LO,12); \
            CONCAT(OP)(HI,LO,13); \
            CONCAT(OP)(HI,LO,14); \
            CONCAT(OP)(HI,LO,15); \
            default: \
                if (ufault_print_info) printf("\x1b[31m%s\x1b[0m","[DASICS]Error: out of libound register range\n"); \
        }

#define JBOUND_REG_READ(hi,lo,idx)  \
        case idx:  \
            lo = csr_read(0x8c0 + idx * 2);  \
            hi = csr_read(0x8c1 + idx * 2);  \
            break;

#define JBOUND_REG_WRITE(hi,lo,idx) \
        case idx:  \
            DASICS_PROLOGUE(); \
            csr_write(0x8c0 + idx * 2, lo);  \
            csr_write(0x8c1 + idx * 2, hi);  \
            break;

#define JCONCAT(OP) JBOUND_REG_##OP

#define JUMPBOUND_LOOKUP(HI,LO,IDX,OP) \
        switch (IDX) \
        {               \
            JCONCAT(OP)(HI,LO,0); \
            JCONCAT(OP)(HI,LO,1); \
            JCONCAT(OP)(HI,LO,2); \
            JCONCAT(OP)(HI,LO,3); \
            default: \
                if (ufault_print_info) printf("\x1b[31m%s\x1b[0m","[DASICS]Error: out of jumpbound register range\n"); \
        }

typedef struct {
    uint64_t lo;
    uint64_t hi;
} bound_t;

/*
 * Software shadow of DASICS boundary registers.
 *
 * Hardware exposes 16 lib boundary slots and 4 jump boundary slots. We mirror
 * each slot in software to decouple allocation bookkeeping from the actual
 * CSR writes:
 *   - allocated: the slot has been reserved by alloc() and not yet freed
 *   - active   : the slot is currently programmed in the corresponding CSRs
 *
 * alloc/free only mutate the software array. dasics_libcfg_active /
 * dasics_jumpcfg_active flush pending changes (allocated != active) into the
 * hardware in a single batched sequence guarded by one DASICS_PROLOGUE.
 */
typedef struct {
    uint64_t lo;
    uint64_t hi;
    uint16_t cfg;        /* lib: V|R|W bits (4 bits used). jump: V bit only. */
    uint8_t  allocated;
    uint8_t  active;
} sw_bound_t;

static sw_bound_t sw_libbounds[DASICS_LIBCFG_WIDTH];
static sw_bound_t sw_jumpbounds[DASICS_JUMPCFG_WIDTH];

void set_ufault_print_info(uint64_t status) 
{
    ufault_print_info = status;
}

/*
 * Read every lib/jump boundary CSR and reconstruct the software shadow so
 * that, immediately after register_udasics(), the software view matches the
 * hardware exactly. Slots whose V bit is unset are reset to a clean state.
 */
static void dasics_sync_from_hw(void)
{
    int32_t step;
    int32_t idx;
    uint64_t libcfg = csr_read(0x880);

    step = 4;
    for (idx = 0; idx < DASICS_LIBCFG_WIDTH; ++idx) {
        uint64_t curr_cfg = (libcfg >> (idx * step)) & DASICS_LIBCFG_MASK;
        if (curr_cfg & DASICS_LIBCFG_V) {
            sw_libbounds[idx].cfg = (uint16_t)curr_cfg;
            sw_libbounds[idx].allocated = 1;
            sw_libbounds[idx].active = 1;
            LIBBOUND_LOOKUP(sw_libbounds[idx].hi, sw_libbounds[idx].lo, idx, READ);
        } else {
            sw_libbounds[idx].cfg = 0;
            sw_libbounds[idx].lo = 0;
            sw_libbounds[idx].hi = 0;
            sw_libbounds[idx].allocated = 0;
            sw_libbounds[idx].active = 0;
        }
    }

    uint64_t jumpcfg = csr_read(0x8c8);

    step = 16;
    for (idx = 0; idx < DASICS_JUMPCFG_WIDTH; ++idx) {
        uint64_t curr_cfg = (jumpcfg >> (idx * step)) & DASICS_JUMPCFG_MASK;
        if (curr_cfg & DASICS_JUMPCFG_V) {
            sw_jumpbounds[idx].cfg = (uint16_t)(curr_cfg & DASICS_JUMPCFG_MASK);
            sw_jumpbounds[idx].allocated = 1;
            sw_jumpbounds[idx].active = 1;
            JUMPBOUND_LOOKUP(sw_jumpbounds[idx].hi, sw_jumpbounds[idx].lo, idx, READ);
        } else {
            sw_jumpbounds[idx].cfg = 0;
            sw_jumpbounds[idx].lo = 0;
            sw_jumpbounds[idx].hi = 0;
            sw_jumpbounds[idx].allocated = 0;
            sw_jumpbounds[idx].active = 0;
        }
    }
}

void register_udasics(uint64_t funcptr) 
{
    umaincall_helper = (funcptr != 0) ? funcptr : (uint64_t) dasics_umaincall_helper;
    DASICS_PROLOGUE();
    csr_write(0x8b0, (uint64_t)dasics_umaincall);
    csr_write(0x005, (uint64_t)dasics_ufault_entry);

    dasics_sync_from_hw();
}

void unregister_udasics(void) 
{
    DASICS_PROLOGUE();
    csr_write(0x8b0, 0);
    csr_write(0x005, 0);    
}

static int bound_coverage_cmp(const void *a, const void *b)
{
    const bound_t *_a = (const bound_t *)a;
    const bound_t *_b = (const bound_t *)b;
    return (_a->lo < _b->lo) ? -1 : 1;
}

static int dasics_bound_checker(uint64_t lo, uint64_t hi, int perm)
{
    // In fact, this is a bound coverage problem for [lo, hi]
    bound_t bounds[DASICS_LIBCFG_WIDTH];
    int32_t idx, items = 0;
    int32_t max_cfgs = DASICS_LIBCFG_WIDTH;

    // Fill bounds array with permission matched libbounds.
    // Source the data from the software shadow: alloc() may have reserved a
    // slot whose CSRs were not yet written, but at the time the fault handler
    // runs, the user must have called dasics_libcfg_active() and the software
    // view is identical to the hardware view.
    for (idx = 0; idx < max_cfgs; ++idx) {
        if (!sw_libbounds[idx].allocated) {
            continue;
        }
        uint32_t cfg = sw_libbounds[idx].cfg & DASICS_LIBCFG_MASK;
        if ((cfg & DASICS_LIBCFG_V) == 0) {
            continue;
        }
        else if ((cfg & (perm | DASICS_LIBCFG_V)) != DASICS_LIBCFG_V) {
            bounds[items].lo = sw_libbounds[idx].lo;
            bounds[items].hi = sw_libbounds[idx].hi;
            items++;
        }
    }

    // Based on the lower bound, sort bounds array in an increasing order
    qsort(bounds, items, sizeof(bound_t), bound_coverage_cmp);

    // Calculate bound coverage via greedy algorithm
    for (idx = 0; idx < items; ++idx) {
        if (bounds[idx].lo <= lo + 1 && lo <= bounds[idx].hi) {
            lo = bounds[idx].hi;
        }
        else if (bounds[idx].hi < lo) {
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
                int perm = (sysno == SYS_read || sysno == SYS_pread) ? DASICS_LIBCFG_W : DASICS_LIBCFG_R;
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
            dasics_free_zone_return_pc = 0x1e264;
            break;
        default:
            printf("\x1b[33m%s\x1b[0m","Warning: Invalid umaincall number %d!\n", type); //could not use printf in kernel
            break;
    }

    DASICS_PROLOGUE();
    csr_write(0x8b1, dasics_return_pc);             // DasicsReturnPC
    csr_write(0x8b2, dasics_free_zone_return_pc);   // DasicsFreeZoneReturnPC
    asm("fence.i");

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
            if (ufault_print_info) printf("[DASICS UEXCEPTION]Info: dasics jump fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx, dfreason = 0x%lx\n", ucause, uepc, utval, dfreason);
            break;
        case EXC_DASICS_LOAD_FAULT:
            if (ufault_print_info) printf("[DASICS UEXCEPTION]Info: dasics load fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx, dfreason = 0x%lx\n", ucause, uepc, utval, dfreason);
            break;
        case EXC_DASICS_STORE_FAULT:
            if (ufault_print_info) printf("[DASICS UEXCEPTION]Info: dasics store fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx, dfreason = 0x%lx\n", ucause, uepc, utval, dfreason);
            break;
        case EXC_DASICS_ECALL_FAULT:
            //if (ufault_print_info) printf("[DASICS EXCEPTION]Info: dasics uecall fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx\n", ucause, uepc, utval);
            if (ufault_print_info) printf("[DASICS UEXCEPTION]Info: dasics lib ecall occurs (ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx, dfreason = 0x%lx), try to check arguments...\n", ucause, uepc, utval, dfreason);
            if(dasics_syscall_checker(sysno, arg1, arg2, arg3, arg4, arg5, arg6)){
                    if (ufault_print_info) printf("[DASICS UEXCEPTION]Info: lib ecall arguments OK! sycall number:%d, syscall is permitted \n", sysno);
                    uint64_t ret = dasics_syscall_proxy(sysno, arg1, arg2, arg3, arg4, arg5, arg6);
                    DASICS_PROLOGUE();
                    csr_write(uepc, uepc + 4);         
                    csr_write(0x8b1, dasics_return_pc);
                    csr_write(0x8b2, dasics_free_zone_return_pc);
                    return;
            } 
            if (ufault_print_info) printf("\x1b[31m%s\x1b[0m","[DASICS UEXCEPTION]Error: lib ecall arguments beyond authority, dasics ecall fault occurs!\n");
            break;
        case EXC_MPK_LOAD_FAULT:
            if (ufault_print_info) printf("[DASICS UEXCEPTION]Info: mpk load fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx, dfreason = 0x%lx\n", ucause, uepc, utval, dfreason);
            break;
        case EXC_MPK_STORE_FAULT:
            if (ufault_print_info) printf("[DASICS UEXCEPTION]Info: mpk store fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx, dfreason = 0x%lx\n", ucause, uepc, utval, dfreason);
            break;
        default:
            if (ufault_print_info) printf("\x1b[31m%s\x1b[0m","[DASICS UEXCEPTION]Error: unexpected dasics fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx, dfreason = 0x%lx\n", ucause, uepc, utval, dfreason);
            break;
    }
    if (ufault_print_info) printf("[DASICS UEXCEPTION]Info: dasics_return_pc:0x%lx\n", dasics_return_pc);	

    // switch base on cause types.
    // currently just skip this inst.
    csr_write(uepc, uepc + 4);
    // rvc will compress jump/branch inst.
    //if (ucause == EXC_DASICS_UFETCH_FAULT)
      //  csr_write(uepc, uepc + 2);
    //else 
       // csr_write(uepc, uepc + 4); 

    // Restore those saved registers
    DASICS_PROLOGUE();
    csr_write(0x8b1, dasics_return_pc);
    csr_write(0x8b2, dasics_free_zone_return_pc);

}

/*
 * Reserve a free lib boundary slot in the software shadow.
 * No CSR is touched here; the caller must invoke dasics_libcfg_active()
 * to push the freshly allocated entry into hardware.
 */
int32_t dasics_libcfg_alloc(uint64_t cfg, uint64_t lo, uint64_t hi) {
    for (int32_t idx = 0; idx < DASICS_LIBCFG_WIDTH; ++idx) {
        if (!sw_libbounds[idx].allocated) {
            sw_libbounds[idx].lo = lo;
            sw_libbounds[idx].hi = hi;
            sw_libbounds[idx].cfg = (uint16_t)((cfg & DASICS_LIBCFG_MASK) | DASICS_LIBCFG_V);
            sw_libbounds[idx].allocated = 1;
            sw_libbounds[idx].active = 0;
            return idx;
        }
    }

    return -1;
}

/*
 * Release a software lib boundary slot. The 'active' flag is intentionally
 * left untouched: if the slot is currently programmed in hardware, the next
 * dasics_libcfg_active() call will detect (allocated=0, active=1) and clear
 * the V bit in the cfg CSR.
 */
int32_t dasics_libcfg_free(int32_t idx) {
    if (idx < 0 || idx >= DASICS_LIBCFG_WIDTH) return -1;
    if (!sw_libbounds[idx].allocated) return -1;

    sw_libbounds[idx].allocated = 0;
    sw_libbounds[idx].cfg = 0;
    return 0;
}

uint32_t dasics_libcfg_get(int32_t idx) {
    if (idx < 0 || idx >= DASICS_LIBCFG_WIDTH) return -1;
    if (!sw_libbounds[idx].allocated) return 0;
    return sw_libbounds[idx].cfg & DASICS_LIBCFG_MASK;
}

/*
 * Flush every pending lib boundary slot (allocated != active) to hardware.
 * All CSR writes form a single batch guarded by one DASICS_PROLOGUE so the
 * underlying privilege gate is asserted exactly once per batch.
 */
void dasics_libcfg_active(void) {
    int32_t step = 4;
    int32_t need_sync = 0;

    for (int32_t idx = 0; idx < DASICS_LIBCFG_WIDTH; ++idx) {
        if (sw_libbounds[idx].allocated != sw_libbounds[idx].active) {
            need_sync = 1;
            break;
        }
    }
    if (!need_sync) return;

    uint64_t libcfg = csr_read(0x880);

    for (int32_t idx = 0; idx < DASICS_LIBCFG_WIDTH; ++idx) {
        if (sw_libbounds[idx].allocated == sw_libbounds[idx].active) {
            continue;
        }

        if (sw_libbounds[idx].allocated) {
            uint64_t lo = sw_libbounds[idx].lo;
            uint64_t hi = sw_libbounds[idx].hi;
            LIBBOUND_LOOKUP(hi, lo, idx, WRITE);

            libcfg &= ~(DASICS_LIBCFG_MASK << (idx * step));
            libcfg |= ((uint64_t)(sw_libbounds[idx].cfg & DASICS_LIBCFG_MASK)) << (idx * step);
            sw_libbounds[idx].active = 1;
        } else {
            libcfg &= ~(DASICS_LIBCFG_V << (idx * step));
            sw_libbounds[idx].active = 0;
        }
    }

    DASICS_PROLOGUE();
    csr_write(0x880, libcfg);
}

int32_t dasics_jumpcfg_alloc(uint64_t lo, uint64_t hi)
{
    for (int32_t idx = 0; idx < DASICS_JUMPCFG_WIDTH; ++idx) {
        if (!sw_jumpbounds[idx].allocated) {
            sw_jumpbounds[idx].lo = lo;
            sw_jumpbounds[idx].hi = hi;
            sw_jumpbounds[idx].cfg = (uint16_t)DASICS_JUMPCFG_V;
            sw_jumpbounds[idx].allocated = 1;
            sw_jumpbounds[idx].active = 0;
            return idx;
        }
    }

    return -1;
}

int32_t dasics_jumpcfg_free(int32_t idx) {
    if (idx < 0 || idx >= DASICS_JUMPCFG_WIDTH) return -1;
    if (!sw_jumpbounds[idx].allocated) return -1;

    sw_jumpbounds[idx].allocated = 0;
    sw_jumpbounds[idx].cfg = 0;
    return 0;
}

void dasics_jumpcfg_active(void) {
    int32_t step = 16;
    int32_t need_sync = 0;

    for (int32_t idx = 0; idx < DASICS_JUMPCFG_WIDTH; ++idx) {
        if (sw_jumpbounds[idx].allocated != sw_jumpbounds[idx].active) {
            need_sync = 1;
            break;
        }
    }
    if (!need_sync) return;

    uint64_t jumpcfg = csr_read(0x8c8);

    for (int32_t idx = 0; idx < DASICS_JUMPCFG_WIDTH; ++idx) {
        if (sw_jumpbounds[idx].allocated == sw_jumpbounds[idx].active) {
            continue;
        }

        if (sw_jumpbounds[idx].allocated) {
            uint64_t lo = sw_jumpbounds[idx].lo;
            uint64_t hi = sw_jumpbounds[idx].hi;
            JUMPBOUND_LOOKUP(hi, lo, idx, WRITE);

            jumpcfg &= ~(DASICS_JUMPCFG_MASK << (idx * step));
            jumpcfg |= ((uint64_t)(sw_jumpbounds[idx].cfg & DASICS_JUMPCFG_MASK)) << (idx * step);
            sw_jumpbounds[idx].active = 1;
        } else {
            jumpcfg &= ~(((uint64_t)DASICS_JUMPCFG_V) << (idx * step));
            sw_jumpbounds[idx].active = 0;
        }
    }

    csr_write(0x8c8, jumpcfg);
}


void dasics_print_cfg_register(int32_t idx)
{
	printf("DASICS uLib CFG Registers: idx:%x  config: %x \n",idx,dasics_libcfg_get(idx));
}
