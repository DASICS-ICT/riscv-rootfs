#include <stdio.h>
#include <unistd.h>
#include <machine/syscall.h>
#include "udasics.h"

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
                printf("\x1b[31m%s\x1b[0m","[DASICS]Error: out of libound register range\n"); \
        }


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

uint32_t dasics_sys_write_checker(uint64_t arg0,uint64_t arg1,uint64_t arg2)
{
    int32_t max_cfgs = DASICS_LIBCFG_WIDTH;
    int32_t idx, is_inbound = 0;
    uint64_t bound_lo_reg,bound_hi_reg;

    for(idx = 0; idx < max_cfgs; idx++){
        LIBBOUND_LOOKUP(bound_hi_reg,bound_lo_reg,idx,READ)

        uint64_t libcfg = dasics_libcfg_get(idx);
        is_inbound = ((libcfg & DASICS_LIBCFG_V) != 0) && ((libcfg & DASICS_LIBCFG_R) != 0) &&
                        (arg1 >= bound_lo_reg) && (arg1 + arg2 <= bound_hi_reg);

        if(is_inbound) return 1;
    }

    return 0;

}


uint32_t dasics_syscall_checker(uint64_t syscall,uint64_t arg0,uint64_t arg1,uint64_t arg2)
{
    switch(syscall)
    {
        case SYS_write:
            return dasics_sys_write_checker(arg0, arg1, arg2);
            break;
        default: //TODO: other syscall implement
            printf("\x1b[33m%s\x1b[0m","[Warning] unsurported syscall for dasics!\n");
            break;
    }
}

uint32_t dasics_syscall_proxy(uint64_t syscall, uint64_t arg0, uint64_t arg1,uint64_t arg2,uint64_t arg3,uint64_t arg4,uint64_t arg5,uint64_t arg6)
{
    switch(syscall)
    {
        //TODO: implementation of more syscalls
        case SYS_write:
            return write((int)arg0, (void *)arg1, (size_t)arg2); 
        default:
            printf("\x1b[33m%s\x1b[0m","[Warning] unsurported syscall for dasics!\n");
            break;
    }
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
        // TODO: print supports variable arguments
        case Umaincall_PRINT: {
            const char *format = va_arg(args, const char *);
            vprintf(format, args);
        }
        case Umaincall_SETAZONERTPC:
            asm volatile ( 
                "li     t0,  0x1d1bc;"\ 
                "csrw   0x8b2, t0;"
                :
                :
                :"t0"
            );     
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

    uint64_t arg0,arg1,arg2,syscall;
    __asm__ volatile(
        "mv t0, a0\n"
        "mv t1, a1\n"
        "mv t2, a2\n"
        "mv t3, a7\n"
        "sd t0, %[arg0]\n"
        "sd t1, %[arg1]\n"
        "sd t2, %[arg2]\n"
        "sd t3, %[syscall]\n"
        : [arg0] "=m" (arg0),[arg1] "=m" (arg1),[arg2] "=m" (arg2), [syscall] "=m" (syscall)
        :
        : "memory"
    );

    switch(ucause)
    {
        case EXC_DASICS_UFETCH_FAULT:
            printf("[DASICS EXCEPTION]Info: dasics ufetch fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx\n", ucause, uepc, utval);
            break;
        case EXC_DASICS_ULOAD_FAULT:
            printf("[DASICS EXCEPTION]Info: dasics uload fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx\n", ucause, uepc, utval);
            break;
        case EXC_DASICS_USTORE_FAULT:
            printf("[DASICS EXCEPTION]Info: dasics ustore fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx\n", ucause, uepc, utval);
            break;
        case EXC_DASICS_UECALL_FAULT:
            //printf("[DASICS EXCEPTION]Info: dasics uecall fault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx\n", ucause, uepc, utval);
            printf("[DASICS EXCEPTION]Info: dasics lib ecall occurs (ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx), try to check arguments...\n", ucause, uepc, utval);
            if(dasics_syscall_checker(syscall,arg0, arg1, arg2)){
                    printf("[DASICS EXCEPTION]Info: lib ecall arguments OK! sycall number:%d, syscall is permitted \n", syscall);   
                    //__asm__ volatile ("ecall\n" );    
                    //TODO: implementation of all syscalls
                    uint32_t ret = dasics_syscall_proxy(syscall, arg0, arg1, arg2, 0, 0, 0,0);
                    csr_write(uepc, uepc + 4);         
                    csr_write(0x8b1, dasics_return_pc);
                    csr_write(0x8b2, dasics_free_zone_return_pc);
                    return;
            } 
            printf("\x1b[31m%s\x1b[0m","[DASICS EXCEPTION]Error: lib ecall arguments beyond authority， dasics ecall fault occurs!\n");
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
    csr_write(0x8b2, dasics_free_zone_return_pc);

}

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
            libcfg |= (cfg & DASICS_LIBCFG_MASK) << (idx * step);
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