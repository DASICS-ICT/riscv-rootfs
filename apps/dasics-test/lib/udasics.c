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

typedef struct {
    uint64_t lo;
    uint64_t hi;
} bound_t;

void set_ufault_print_info(uint64_t status) 
{
    ufault_print_info = status;
}

void register_udasics(uint64_t funcptr) 
{
    umaincall_helper = (funcptr != 0) ? funcptr : (uint64_t) dasics_umaincall_helper;
    csr_write(0x8b0, (uint64_t)dasics_umaincall);
#ifndef DASICS_S_TRAP_ONLY
    csr_write(0x005, (uint64_t)dasics_ufault_entry);
#endif
}

void unregister_udasics(void) 
{
    csr_write(0x8b0, 0);
#ifndef DASICS_S_TRAP_ONLY
    csr_write(0x005, 0);    
#endif
}

static int bound_coverage_cmp(const void *a, const void *b)
{
    const bound_t *_a = (const bound_t *)a;
    const bound_t *_b = (const bound_t *)b;

    if (_a->lo < _b->lo) return -1;
    if (_a->lo > _b->lo) return 1;
    if (_a->hi < _b->hi) return -1;
    if (_a->hi > _b->hi) return 1;
    return 0;
}

static int dasics_bound_checker(uint64_t start, uint64_t end, int perm)
{
    // Bound CSRs represent half-open ranges after the 8-byte WARL mask.
    bound_t bounds[DASICS_LIBCFG_WIDTH];
    uint64_t cursor = start;
    uint32_t required_cfg = (uint32_t)perm | DASICS_LIBCFG_V;
    int32_t idx, items = 0;
    int32_t max_cfgs = DASICS_LIBCFG_WIDTH;

    if (end < start) return 0;
    if (end == start) return 1;

    for (idx = 0; idx < max_cfgs; ++idx) {
        uint32_t cfg = dasics_libcfg_get(idx);
        if ((cfg & required_cfg) != required_cfg) continue;

        LIBBOUND_LOOKUP(bounds[items].hi, bounds[items].lo, idx, READ);
        items++;
    }

    qsort(bounds, items, sizeof(bound_t), bound_coverage_cmp);

    for (idx = 0; idx < items; ++idx) {
        if (bounds[idx].lo >= bounds[idx].hi ||
            bounds[idx].hi <= cursor) continue;
        if (bounds[idx].lo > cursor) break;

        cursor = bounds[idx].hi;
        if (cursor >= end) return 1;
    }

    return 0;
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
                uint64_t start = (uint64_t)arg2;
                uint64_t end = start + (uint64_t)arg3;

                retval = dasics_bound_checker(start, end, perm);
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

#ifdef DASICS_N_EXTENSION_PROFILE
volatile uint64_t dasics_n_extension_syscall_trap_count;
volatile uint64_t dasics_n_extension_syscall_permitted_count;
volatile uint64_t dasics_n_extension_syscall_denied_count;
volatile dasics_n_extension_syscall_record_t
    dasics_n_extension_syscall_records[DASICS_N_EXTENSION_SYSCALL_RECORDS];

static uint32_t dasics_n_extension_syscall_permitted(SYSCALL_ARGS)
{
    switch (sysno) {
    case SYS_read:
    case SYS_write:
    case SYS_pread:
    case SYS_pwrite:
        return dasics_syscall_checker(
            sysno, arg1, arg2, arg3, arg4, arg5, arg6);
    default:
        return 0;
    }
}

long dasics_n_extension_syscall_handler(SYSCALL_ARGS)
{
    uint64_t index = dasics_n_extension_syscall_trap_count;
    uint32_t permitted = dasics_n_extension_syscall_permitted(
        sysno, arg1, arg2, arg3, arg4, arg5, arg6);
    volatile dasics_n_extension_syscall_record_t *record = NULL;
    long result;

    if (index < DASICS_N_EXTENSION_SYSCALL_RECORDS) {
        record = &dasics_n_extension_syscall_records[index];
        record->ustatus = csr_read(ustatus);
        record->uepc = csr_read(uepc);
        record->ucause = csr_read(ucause);
        record->utval = csr_read(utval);
        record->dfreason = csr_read(CSR_DFREASON);
        record->permitted = permitted;
    }
    dasics_n_extension_syscall_trap_count = index + 1;

    if (!permitted) {
        dasics_n_extension_syscall_denied_count++;
        result = -1;
    } else {
        dasics_n_extension_syscall_permitted_count++;
        /*
         * This function and dasics_syscall_proxy reside in trusted .text.  The
         * reissued ecall is therefore an ordinary U-mode syscall (cause 8),
         * which remains delegated to HS rather than looping back into HU.
         */
        result = dasics_syscall_proxy(
            sysno, arg1, arg2, arg3, arg4, arg5, arg6);
    }

    if (record != NULL) {
        record->result = result;
    }
    return result;
}
#endif

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
            break;
        }
        case Umaincall_SETAZONERTPC:
            /* dasics_umaincall derives the free-zone return PC from its caller. */
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

long dasics_complete_app_getpid(void)
{
    register uint64_t a0 asm("a0") = 0;
    register uint64_t a7 asm("a7") = DASICS_COMPLETE_APP_GETPID;

    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return (long)a0;
}

long dasics_complete_app_control(uint64_t command, uint64_t pid,
                                 uint64_t stage, uint64_t value)
{
    register uint64_t a0 asm("a0") = DASICS_COMPLETE_APP_CONTROL_MAGIC;
    register uint64_t a1 asm("a1") = command;
    register uint64_t a2 asm("a2") = pid;
    register uint64_t a3 asm("a3") = stage;
    register uint64_t a4 asm("a4") = value;
    register uint64_t a7 asm("a7") = DASICS_COMPLETE_APP_GETPID;

    asm volatile("ecall"
                 : "+r"(a0)
                 : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a7)
                 : "memory");
    return (long)a0;
}

#ifndef DASICS_S_TRAP_ONLY
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
    csr_write(0x8b1, dasics_return_pc);
    csr_write(0x8b2, dasics_free_zone_return_pc);

}
#endif

int32_t dasics_libcfg_alloc(uint64_t cfg, uint64_t lo, uint64_t hi) {
    uint64_t libcfg = csr_read(0x880);  // DasicsLibCfg
    int32_t max_cfgs = DASICS_LIBCFG_WIDTH;
    int32_t step = 4;

    for (int32_t idx = 0; idx < max_cfgs; ++idx) {
        uint64_t curr_cfg = (libcfg >> (idx * step)) & DASICS_LIBCFG_MASK;

        if ((curr_cfg & DASICS_LIBCFG_V) == 0)  // Found available config
        {
            // Write DASICS bounds csr
            switch (idx) {
                case 0:
                    csr_write(0x890, lo);   // DasicsLibBound0Lo
                    csr_write(0x891, hi);   // DasicsLibBound0Hi
                    break;
                case 1:
                    csr_write(0x892, lo);   // DasicsLibBound1Lo
                    csr_write(0x893, hi);   // DasicsLibBound1Hi
                    break;
                case 2:
                    csr_write(0x894, lo);   // DasicsLibBound2Lo
                    csr_write(0x895, hi);   // DasicsLibBound2Hi
                    break;
                case 3:
                    csr_write(0x896, lo);   // DasicsLibBound3Lo
                    csr_write(0x897, hi);   // DasicsLibBound3Hi
                    break;
                case 4:
                    csr_write(0x898, lo);   // DasicsLibBound4Lo
                    csr_write(0x899, hi);   // DasicsLibBound4Hi
                    break;
                case 5:
                    csr_write(0x89a, lo);   // DasicsLibBound5Lo
                    csr_write(0x89b, hi);   // DasicsLibBound5Hi
                    break;
                case 6:
                    csr_write(0x89c, lo);   // DasicsLibBound6Lo
                    csr_write(0x89d, hi);   // DasicsLibBound6Hi
                    break;
                case 7:
                    csr_write(0x89e, lo);   // DasicsLibBound7Lo
                    csr_write(0x89f, hi);   // DasicsLibBound7Hi
                    break;
                case 8:
                    csr_write(0x8a0, lo);   // DasicsLibBound8Lo
                    csr_write(0x8a1, hi);   // DasicsLibBound8Hi
                    break;
                case 9:
                    csr_write(0x8a2, lo);   // DasicsLibBound9Lo
                    csr_write(0x8a3, hi);   // DasicsLibBound9Hi
                    break;
                case 10:
                    csr_write(0x8a4, lo);   // DasicsLibBound10Lo
                    csr_write(0x8a5, hi);   // DasicsLibBound10Hi
                    break;
                case 11:
                    csr_write(0x8a6, lo);   // DasicsLibBound11Lo
                    csr_write(0x8a7, hi);   // DasicsLibBound11Hi
                    break;
                case 12:
                    csr_write(0x8a8, lo);   // DasicsLibBound12Lo
                    csr_write(0x8a9, hi);   // DasicsLibBound12Hi
                    break;
                case 13:
                    csr_write(0x8aa, lo);   // DasicsLibBound13Lo
                    csr_write(0x8ab, hi);   // DasicsLibBound13Hi
                    break;
                case 14:
                    csr_write(0x8ac, lo);   // DasicsLibBound14Lo
                    csr_write(0x8ad, hi);   // DasicsLibBound14Hi
                    break;
                case 15:
                    csr_write(0x8ae, lo);   // DasicsLibBound15Lo
                    csr_write(0x8af, hi);   // DasicsLibBound15Hi
                    break;
                default:
                    break;
            }

            // Write config
            libcfg &= ~(DASICS_LIBCFG_MASK << (idx * step));
            libcfg |= ((cfg & DASICS_LIBCFG_MASK) | DASICS_LIBCFG_V) << (idx * step);
            csr_write(0x880, libcfg);   // DasicsLibCfg

            return idx;
        }
    }

    return -1;
}

int32_t dasics_libcfg_free(int32_t idx) {
    if (idx < 0 || idx >= DASICS_LIBCFG_WIDTH) return -1;

    int32_t step = 4;
    uint64_t libcfg = csr_read(0x880);  // DasicsLibCfg
    libcfg &= ~(DASICS_LIBCFG_V << (idx * step));
    csr_write(0x880, libcfg);   // DasicsLibCfg
    return 0;
}

uint32_t dasics_libcfg_get(int32_t idx) {
    if (idx < 0 || idx >= DASICS_LIBCFG_WIDTH) return -1;

    int32_t step = 4;
    uint64_t libcfg = csr_read(0x880);  // DasicsLibCfg
    return (libcfg >> (idx * step)) & DASICS_LIBCFG_MASK;
}

int32_t dasics_jumpcfg_alloc(uint64_t lo, uint64_t hi)
{
    uint64_t jumpcfg = csr_read(0x8c8);    // DasicsJumpCfg
    int32_t max_cfgs = DASICS_JUMPCFG_WIDTH;
    int32_t step = 16;

    for (int32_t idx = 0; idx < max_cfgs; ++idx) {
        uint64_t curr_cfg = (jumpcfg >> (idx * step)) & DASICS_JUMPCFG_MASK;
        if ((curr_cfg & DASICS_JUMPCFG_V) == 0) // found available cfg
        {
            // Write DASICS jump boundary CSRs
            switch (idx) {
                case 0:
                    csr_write(0x8c0, lo);  // DasicsJumpBound0Lo
                    csr_write(0x8c1, hi);  // DasicsJumpBound0Hi
                    break;
                case 1:
                    csr_write(0x8c2, lo);  // DasicsJumpBound1Lo
                    csr_write(0x8c3, hi);  // DasicsJumpBound1Hi
                    break;
                case 2:
                    csr_write(0x8c4, lo);  // DasicsJumpBound2Lo
                    csr_write(0x8c5, hi);  // DasicsJumpBound2Hi
                    break;
                case 3:
                    csr_write(0x8c6, lo);  // DasicsJumpBound3Lo
                    csr_write(0x8c7, hi);  // DasicsJumpBound3Hi
                    break;
                default:
                    break;
            }

            jumpcfg &= ~(DASICS_JUMPCFG_MASK << (idx * step));
            jumpcfg |= DASICS_JUMPCFG_V << (idx * step);
            csr_write(0x8c8, jumpcfg); // DasicsJumpCfg

            return idx;
        }
    }

    return -1;
}

int32_t dasics_jumpcfg_free(int32_t idx) {
    if (idx < 0 || idx >= DASICS_JUMPCFG_WIDTH) {
        return -1;
    }

    int32_t step = 16;
    uint64_t jumpcfg = csr_read(0x8c8);    // DasicsJumpCfg
    jumpcfg &= ~(DASICS_JUMPCFG_V << (idx * step));
    csr_write(0x8c8, jumpcfg); // DasicsJumpCfg
    return 0;
}


void dasics_print_cfg_register(int32_t idx)
{
	printf("DASICS uLib CFG Registers: idx:%x  config: %x \n",idx,dasics_libcfg_get(idx));
}
