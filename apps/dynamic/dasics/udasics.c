#include <stdio.h>
#include <stdint.h>
#include <udasics.h>
#include <my_stdio.h>
#include <utrap.h>
#include <utrap_handler.h>
#include <dasics_link_manager.h>
#include <cross.h>

uint64_t umaincall_helper;

void register_udasics(uint64_t funcptr) 
{
    umaincall_helper = (funcptr != 0) ? funcptr : (uint64_t) dasics_umaincall_helper;
    csr_write(0x8a3, (uint64_t)dasics_umaincall);
    csr_write(0x005, (uint64_t)dasics_ufault_entry);
}

void unregister_udasics(void) 
{
    csr_write(0x8a3, 0);
    csr_write(0x005, 0);    
}

// uint64_t dasics_umaincall_helper(UmaincallTypes type, uint64_t arg0, uint64_t arg1, uint64_t arg2)
// {
//     uint64_t dasics_return_pc = csr_read(0x8a4);            // DasicsReturnPC
//     uint64_t dasics_free_zone_return_pc = csr_read(0x8a5);  // DasicsFreeZoneReturnPC

//     uint64_t retval = 0;

//     switch (type)
//     {
//         // TODO: print supports variable arguments
//         case Umaincall_PRINT:
//             if (!arg0) break;
//             if (arg1) goto print_arg1; 
//             // printf((char *)arg0);
//             break;
//         print_arg1:
//             if (arg2) goto print_arg2;
//             // printf((char *)arg0, arg1);
//             break;
//         print_arg2:
//             // printf((char *)arg0, arg1, arg2);
//             break;
//         default:
//             // printf("Warning: Invalid umaincall number %u!\n", type); //could not use // printf in kernel
//             break;
//     }

//     csr_write(0x8a4, dasics_return_pc);             // DasicsReturnPC
//     csr_write(0x8a5, dasics_free_zone_return_pc);   // DasicsFreeZoneReturnPC

//     return retval;
// }

// unsigned long ATTR_ULIB_TEXT _generate_addr() {
// 	int i = 0xf;
// 	unsigned long ptr = 0; 
// 	while (i > 0) {
// 		ptr |= i;
// 		ptr <<= 4;
// 		--i;
// 	}
// 	return ptr;
// }


// NOP instruction
#define NOP 0x00000013
/* Now used for dynamic call, never used dynamic lib func!!! */
uint64_t dasics_umaincall_helper(regs_context_t * r_regs)
{
    /* judge t1 to configure where the t1 come from 
     * auipc	t3,0x2
     * ld	t3,-1072(t3) # 2020 <free>
     * jalr	t1,t3
     * nop
     */
    umain_got_t *entry = _get_trap_area(r_regs->t1);
    // if t3 equal to (uint64_t)dasics_umaincall and 
    // entry not null, it will be a dynamic external
    // function call
    if (entry && r_regs->t3 == (uint64_t)dasics_umaincall && *(uint32_t *)r_regs->t1 == NOP)
    {    
        dasics_dynamic_call(entry, r_regs);
        return 0;
    }
    if (r_regs->ra == (uint64_t)dasics_umaincall)
    {
        dasics_dynamic_return(r_regs);
        return 0;
    }

    if (entry == NULL)
    {
    #ifdef DASICS_DEBUG
        my_printf("[ERROR] DASICS error t1: 0x%lx!\n", r_regs->t1);
    #endif
        exit(1);
    }

    goto other_service;


other_service:
    // TODO: add other service, such as printf for untrusted area.
    // the other service, maybe we can save a magic number to one
    // register to distinguish other_service
    
    // int type = r_regs->a0;
    // uint64_t arg0 = r_regs->a1;
    // uint64_t arg1 = r_regs->a2;
    // uint64_t arg2 = r_regs->a3;
    // uint64_t arg3 = r_regs->a3;


    // switch (type)
    // {
    //     // TODO: print supports variable arguments
    //     case Umaincall_PRINT:
    //         if (!arg0) break;
    //         if (arg1) goto print_arg1; 
    //         printf((char *)arg0);
    //         break;
    //     print_arg1:
    //         if (arg2) goto print_arg2;
    //         printf((char *)arg0, arg1);
    //         break;
    //     print_arg2:
    //         printf((char *)arg0, arg1, arg2);
    //         break;
    //     default:
    //         printf("Warning: Invalid umaincall number %u!\n", type); //could not use // printf in kernel
    //         break;
    // }

    ;

}

#pragma GCC push_options
#pragma GCC optimize("O0")

void die_for()
{
    int i = 0x100000;
    while(i-- > 0);
}


#pragma GCC pop_options


void dasics_ufault_handler(regs_context_t * regs)
{
    // Save some registers that should be saved by callees
    int error;
    switch (regs->ucause)
    {
    case DasicsUFetchFault:
        error = handle_DasicsUFetchFault(regs);
        break;
    
    case DasicsULoadFault:
        error = handle_DasicsULoadFault(regs);
        break;

    case DasicsUStoreFault:
        error = handle_DasicsUStoreFault(regs);
        break;
    
    case DasicsEcallFault:
        error = handle_DasicsUEcallFault(regs);
        break;
        
    default:
        my_printf("[ERROR] unhandle ufault: 0x%lx\n", regs->ucause);
        while(1);
        break;
    }
    asm volatile("fence\n");
    if (error == -1)
        exit(1);
}

int32_t dasics_libcfg_alloc(uint64_t cfg, uint64_t hi, uint64_t lo)
{
    uint64_t libcfg0 = csr_read(0x881);  // DasicsLibCfg0
    uint64_t libcfg1 = csr_read(0x882);  // DasicsLibCfg1

    int32_t max_cfgs = DASICS_LIBCFG_WIDTH << 1;
#ifndef RV32
    int32_t step = 8;  // rv64
#else
    int32_t step = 4;  // rv32
#endif

    int32_t idx;

    for (idx = 0; idx < max_cfgs; ++idx)
    {
        int choose_libcfg0 = (idx < DASICS_LIBCFG_WIDTH);
        int32_t _idx = choose_libcfg0 ? idx : idx - DASICS_LIBCFG_WIDTH;
        uint64_t curr_cfg = ((choose_libcfg0 ? libcfg0 : libcfg1) >> (_idx * step)) & DASICS_LIBCFG_MASK;

        if ((curr_cfg & DASICS_LIBCFG_V) == 0)  // Find avaliable cfg
        {
            if (choose_libcfg0)
            {
                libcfg0 &= ~(DASICS_LIBCFG_MASK << (_idx * step));
                libcfg0 |= (cfg & DASICS_LIBCFG_MASK) << (_idx * step);
                csr_write(0x881, libcfg0);  // DasicsLibCfg0
            }
            else
            {
                libcfg1 &= ~(DASICS_LIBCFG_MASK << (_idx * step));
                libcfg1 |= (cfg & DASICS_LIBCFG_MASK) << (_idx * step);
                csr_write(0x882, libcfg1);  // DasicsLibCfg1
            }

            // Write DASICS boundary csrs            
            switch (idx)
            {
                case 0:
                    csr_write(0x883, hi);  // DasicsLibBound0
                    csr_write(0x884, lo);  // DasicsLibBound1
                    break;
                case 1:
                    csr_write(0x885, hi);  // DasicsLibBound2
                    csr_write(0x886, lo);  // DasicsLibBound3
                    break;
                case 2:
                    csr_write(0x887, hi);  // DasicsLibBound4
                    csr_write(0x888, lo);  // DasicsLibBound5
                    break;
                case 3:
                    csr_write(0x889, hi);  // DasicsLibBound6
                    csr_write(0x88a, lo);  // DasicsLibBound7
                    break;
                case 4:
                    csr_write(0x88b, hi);  // DasicsLibBound8
                    csr_write(0x88c, lo);  // DasicsLibBound9
                    break;
                case 5:
                    csr_write(0x88d, hi);  // DasicsLibBound10
                    csr_write(0x88e, lo);  // DasicsLibBound11
                    break;
                case 6:
                    csr_write(0x88f, hi);  // DasicsLibBound12
                    csr_write(0x890, lo);  // DasicsLibBound13
                    break;
                case 7:
                    csr_write(0x891, hi);  // DasicsLibBound14
                    csr_write(0x892, lo);  // DasicsLibBound15
                    break;
                case 8:
                    csr_write(0x893, hi);  // DasicsLibBound16
                    csr_write(0x894, lo);  // DasicsLibBound17
                    break;
                case 9:
                    csr_write(0x895, hi);  // DasicsLibBound18
                    csr_write(0x896, lo);  // DasicsLibBound19
                    break;
                case 10:
                    csr_write(0x897, hi);  // DasicsLibBound20
                    csr_write(0x898, lo);  // DasicsLibBound21
                    break;
                case 11:
                    csr_write(0x899, hi);  // DasicsLibBound22
                    csr_write(0x89a, lo);  // DasicsLibBound23
                    break;
                case 12:
                    csr_write(0x89b, hi);  // DasicsLibBound24
                    csr_write(0x89c, lo);  // DasicsLibBound25
                    break;
                case 13:
                    csr_write(0x89d, hi);  // DasicsLibBound26
                    csr_write(0x89e, lo);  // DasicsLibBound27
                    break;
                case 14:
                    csr_write(0x89f, hi);  // DasicsLibBound28
                    csr_write(0x8a0, lo);  // DasicsLibBound29
                    break;
                default:
                    csr_write(0x8a1, hi);  // DasicsLibBound30
                    csr_write(0x8a2, lo);  // DasicsLibBound31
                    break;
            }
            
            return idx;
        }
    }

    return -1;
}

int32_t dasics_libcfg_free(int32_t idx)
{
    if (idx < 0 || idx >= (DASICS_LIBCFG_WIDTH << 1))
    {
        return -1;
    }

#ifndef RV32
    int32_t step = 8;  // rv64
#else
    int32_t step = 4;  // rv32
#endif

    int choose_libcfg0 = (idx < DASICS_LIBCFG_WIDTH);
    int32_t _idx = choose_libcfg0 ? idx : idx - DASICS_LIBCFG_WIDTH;

    uint64_t libcfg = choose_libcfg0 ? csr_read(0x881):  // DasicsLibCfg0
                                       csr_read(0x882);  // DasicsLibCfg1
    libcfg &= ~(DASICS_LIBCFG_V << (_idx * step));

    if (choose_libcfg0)
    {
        csr_write(0x881, libcfg);  // DasicsLibCfg0
    }
    else
    {
        csr_write(0x882, libcfg);  // DasicsLibCfg1
    }

    return 0;
}

uint32_t dasics_libcfg_get(int32_t idx)
{
    if (idx < 0 || idx >= (DASICS_LIBCFG_WIDTH << 1))
    {
        return -1;
    }

#ifndef RV32
    int32_t step = 8;  // rv64
#else
    int32_t step = 4;  // rv32
#endif

    int choose_libcfg0 = (idx < DASICS_LIBCFG_WIDTH);
    int32_t _idx = choose_libcfg0 ? idx : idx - DASICS_LIBCFG_WIDTH;

    uint64_t libcfg = choose_libcfg0 ? csr_read(0x881):  // DasicsLibCfg0
                                       csr_read(0x882);  // DasicsLibCfg1

    return (libcfg >> (_idx * step)) & DASICS_LIBCFG_MASK;
}
