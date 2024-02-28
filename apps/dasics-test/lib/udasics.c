#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <machine/syscall.h>
#include "udasics.h"
#include "uthash.h"

uint64_t umaincall_helper;

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
            default: \
                printf("\x1b[31m%s\x1b[0m","[DASICS]Error: out of libound register range\n"); \
        }

typedef struct {
    uint64_t lo;
    uint64_t hi;
} bound_t;

typedef struct {
    int handle;  /* key */
    int priv;
    bound_t bound;
    UT_hash_handle hh;
} hashed_bound_t;

hashed_bound_t *bounds_table = NULL;
static int available_handle = 0;  // FIXME: Currently we ignore int overflow conditions

int dlibcfg_handle_map[DASICS_LIBCFG_WIDTH] = {-1};
// 0x241a7
void register_udasics(uint64_t funcptr) 
{
    uint64_t libcfg = csr_read(0x880);  // DasicsLibCfg
    int32_t max_cfgs = DASICS_LIBCFG_WIDTH;
    int32_t step = 4;

    // Set random seed
    srand(2023);

    // Write OS-allocated bounds to hash table
    for (int32_t idx = 0; idx < max_cfgs; ++idx) {
        uint64_t curr_cfg = (libcfg >> (idx * step)) & DASICS_LIBCFG_MASK;

        // Found allocated config
        if ((curr_cfg & DASICS_LIBCFG_V) != 0) {
            uint64_t hi, lo;
            LIBBOUND_LOOKUP(hi, lo, idx, READ);
            hashed_bound_t *entry = (hashed_bound_t *)malloc(sizeof(hashed_bound_t));
            entry->bound.hi = hi;
            entry->bound.lo = lo;
            entry->priv = curr_cfg;
            entry->handle = available_handle++;
            HASH_ADD_INT(bounds_table, handle, entry);
            dlibcfg_handle_map[idx] = entry->handle;
        }
    }

    // Set maincall & ufault handler
    umaincall_helper = (funcptr != 0) ? funcptr : (uint64_t) dasics_umaincall_helper;
    csr_write(0x8b0, (uint64_t)dasics_umaincall);
    // csr_write(0x005, (uint64_t)dasics_ufault_entry);
}

void unregister_udasics(void) 
{
    csr_write(0x8b0, 0);
    // csr_write(0x005, 0);

    // Free bounds hash table
    hashed_bound_t *current, *temp;
    HASH_ITER(hh, bounds_table, current, temp) {
        HASH_DEL(bounds_table, current);
        free(current);
    }
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

    // Fill bounds array with permission matched libbounds
    for (idx = 0; idx < max_cfgs; ++idx) {
        uint32_t cfg = dasics_libcfg_get(idx);
        if (cfg == -1 || (cfg & DASICS_LIBCFG_V) == 0) {
            continue;
        }
        else if ((cfg & (perm | DASICS_LIBCFG_V)) != DASICS_LIBCFG_V) {
            // Permission matched, add this libbound to bound list
            LIBBOUND_LOOKUP(bounds[items].hi, bounds[items].lo, idx, READ);
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
    // uint64_t dasics_free_zone_return_pc = csr_read(0x8b2);  // DasicsFreeZoneReturnPC

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
            asm volatile (
                "li     t0,  0x21860\n"
                "csrw   0x8b2, t0\n"
                :::"t0"
            );
            break;

        default:
            printf("\x1b[33m%s\x1b[0m","Warning: Invalid umaincall number %d!\n", type); //could not use printf in kernel
            break;
    }

    csr_write(0x8b1, dasics_return_pc);             // DasicsReturnPC
    // csr_write(0x8b2, dasics_free_zone_return_pc);   // DasicsFreeZoneReturnPC

    va_end(args);

    return retval;
}

/**
 * Check whether the bounds hash table contains the corresponding entry
 * If so, load the entry to dlibcfg and dlibbounds
 * @param utval faulting memory address
 * @param is_read this operation is read or write
 * @return -1 if cannot find hash table entry; newly inserted dlibcfg idx if found
 */
static int dasics_ldst_checker(uint64_t utval, int is_read)
{
    int valid_perm = DASICS_LIBCFG_V | (is_read ? DASICS_LIBCFG_R : DASICS_LIBCFG_W);
    uint64_t libcfg = csr_read(0x880);   // DasicsLibCfg
    int step = 4;

    // Iterate bounds table to find matching bounds
    hashed_bound_t *current, *temp;
    HASH_ITER(hh, bounds_table, current, temp) {
        if (current->bound.lo <= utval && utval <= current->bound.hi && \
            (current->priv & valid_perm) == valid_perm) {
            // Find the matching bound, thus replace one libcfg & libbound with it
            int victim = rand() % DASICS_LIBCFG_WIDTH;
            LIBBOUND_LOOKUP(current->bound.hi, current->bound.lo, victim, WRITE);

            // Write config
            libcfg &= ~(DASICS_LIBCFG_MASK << (victim * step));
            libcfg |= ((uint64_t)current->priv) << (victim * step);
            csr_write(0x880, libcfg);   // DasicsLibCfg

             // Fill dlibcsr map with new handle
            dlibcfg_handle_map[victim] = current->handle;
            
            return victim;
        }
    }

    return -1;
}

void dasics_ufault_handler(void)
{
    // Save some registers that should be saved by callees
    uint64_t dasics_return_pc = csr_read(0x8b1);
    // uint64_t dasics_free_zone_return_pc = csr_read(0x8b2);

    uint64_t ucause = csr_read(ucause);
    uint64_t utval = csr_read(utval);
    uint64_t uepc = csr_read(uepc);

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

    switch(ucause)
    {
        case EXC_DASICS_UFETCH_FAULT:
            printf("[DASICS EXCEPTION]Info: dasics ufetch fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx\n", ucause, uepc, utval);
            break;
        case EXC_DASICS_ULOAD_FAULT: case EXC_DASICS_USTORE_FAULT: {
            int is_uload = (ucause == EXC_DASICS_ULOAD_FAULT);
            printf("[DASICS EXCEPTION]Info: dasics %s fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx\n", \
                is_uload ? "uload" : "ustore", ucause, uepc, utval);
            int csr_idx = dasics_ldst_checker(utval, is_uload);
            if (0 <= csr_idx && csr_idx < DASICS_LIBCFG_WIDTH) {
                uint64_t hi, lo;
                LIBBOUND_LOOKUP(hi, lo, csr_idx, READ);
                printf("[DASICS EXCEPTION]Info: dasics %s fault OK! new csr idx is %d, lo = %#lx, hi = %#lx\n", \
                    is_uload ? "uload" : "ustore", csr_idx, lo, hi);
                csr_write(0x8b1, dasics_return_pc);
                // csr_write(0x8b2, dasics_free_zone_return_pc);
                return;
            }
            break;
        }
        case EXC_DASICS_UECALL_FAULT:
            //printf("[DASICS EXCEPTION]Info: dasics uecall fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx\n", ucause, uepc, utval);
            printf("[DASICS EXCEPTION]Info: dasics lib ecall occurs (ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx), try to check arguments...\n", ucause, uepc, utval);
            if(dasics_syscall_checker(sysno, arg1, arg2, arg3, arg4, arg5, arg6)){
                    printf("[DASICS EXCEPTION]Info: lib ecall arguments OK! sycall number:%d, syscall is permitted \n", sysno);
                    uint64_t ret = dasics_syscall_proxy(sysno, arg1, arg2, arg3, arg4, arg5, arg6);
                    csr_write(uepc, uepc + 4);         
                    csr_write(0x8b1, dasics_return_pc);
                    // csr_write(0x8b2, dasics_free_zone_return_pc);
                    return;
            } 
            printf("\x1b[31m%s\x1b[0m","[DASICS EXCEPTION]Error: lib ecall arguments beyond authority, dasics ecall fault occurs!\n");
            break;
        default:
            printf("\x1b[31m%s\x1b[0m","[DASICS EXCEPTION]Error: unexpected dasics fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx\n", ucause, uepc, utval);
            break;
    }
    printf("[DASICS EXCEPTION]Info: dasics_return_pc:0x%lx\n", dasics_return_pc);	

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
    // csr_write(0x8b2, dasics_free_zone_return_pc);

}

int32_t dasics_libcfg_alloc(uint64_t cfg, uint64_t lo, uint64_t hi) {
    uint64_t libcfg = csr_read(0x880);  // DasicsLibCfg
    int32_t max_cfgs = DASICS_LIBCFG_WIDTH;
    int32_t step = 4;

    // Insert new bound information to hash table
    hashed_bound_t *entry = (hashed_bound_t *)malloc(sizeof(hashed_bound_t));
    entry->bound.hi = hi;
    entry->bound.lo = lo;
    entry->priv = (cfg & DASICS_LIBCFG_MASK) | DASICS_LIBCFG_V;
    entry->handle = available_handle++;
    HASH_ADD_INT(bounds_table, handle, entry);

    // Find a proper libcfg for the newly allocated bound
    int32_t victim = 0;
    for (; victim < max_cfgs; ++victim) {
        uint64_t curr_cfg = (libcfg >> (victim * step)) & DASICS_LIBCFG_MASK;

        // Found available config
        if ((curr_cfg & DASICS_LIBCFG_V) == 0) {
            break;
        }
    }

    // Randomly kick out one victim if we cannot find one available place
    if (victim == max_cfgs) {
        victim = rand() % DASICS_LIBCFG_WIDTH;
    }

    // Write libbound
    LIBBOUND_LOOKUP(hi, lo, victim, WRITE);

    // Write config
    libcfg &= ~(DASICS_LIBCFG_MASK << (victim * step));
    libcfg |= ((uint64_t)entry->priv) << (victim * step);
    csr_write(0x880, libcfg);   // DasicsLibCfg

    // Fill dlibcsr map with new handle
    dlibcfg_handle_map[victim] = entry->handle;

    return entry->handle;
}

int32_t dasics_libcfg_free(int32_t handle) {
    if (handle < 0) {
        return -1;
    }

    // Lookup hashed table firstly
    hashed_bound_t *entry;
    HASH_FIND_INT(bounds_table, &handle, entry);

    if (NULL == entry) {
        return -1;
    }

    // Erase hash map entry
    HASH_DEL(bounds_table, entry);
    free(entry);

    // Check if the target bound exists in dasics CSRs
    int32_t idx = 0;
    int32_t step = 4;
    for (; idx < DASICS_LIBCFG_WIDTH; ++idx) {
        if (dlibcfg_handle_map[idx] == handle) {
            uint64_t libcfg = csr_read(0x880);  // DasicsLibCfg
            libcfg &= ~(DASICS_LIBCFG_V << (idx * step));
            csr_write(0x880, libcfg);   // DasicsLibCfg
            dlibcfg_handle_map[idx] = -1;
            break;
        }
    }

    return 0;
}

uint32_t dasics_libcfg_get(int32_t handle) {
    hashed_bound_t *entry;
    HASH_FIND_INT(bounds_table, &handle, entry);

    if (NULL == entry) {
        return -1;
    } else {
        return entry->priv;
    }
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
                // case 1:
                //     csr_write(0x8c2, lo);  // DasicsJumpBound1Lo
                //     csr_write(0x8c3, hi);  // DasicsJumpBound1Hi
                //     break;
                // case 2:
                //     csr_write(0x8c4, lo);  // DasicsJumpBound2Lo
                //     csr_write(0x8c5, hi);  // DasicsJumpBound2Hi
                //     break;
                // case 3:
                //     csr_write(0x8c6, lo);  // DasicsJumpBound3Lo
                //     csr_write(0x8c7, hi);  // DasicsJumpBound3Hi
                //     break;
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


void dasics_print_cfg_register(int32_t handle)
{
	printf("DASICS uLib CFG Registers: handle:%x  config: %x \n",handle,dasics_libcfg_get(handle));
}