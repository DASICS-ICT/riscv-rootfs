#include <stdio.h>
#include "udasics.h"

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

uint64_t dasics_umaincall_helper(UmaincallTypes type, uint64_t arg0, uint64_t arg1, uint64_t arg2)
{
    uint64_t dasics_return_pc = csr_read(0x8a4);            // DasicsReturnPC
    uint64_t dasics_free_zone_return_pc = csr_read(0x8a5);  // DasicsFreeZoneReturnPC

    uint64_t retval = 0;

    switch (type)
    {
        // TODO: print supports variable arguments
        case Umaincall_PRINT:
            if (!arg0) break;
            if (arg1) goto print_arg1; 
            printf((char *)arg0);
            break;
        print_arg1:
            if (arg2) goto print_arg2;
            printf((char *)arg0, arg1);
            break;
        print_arg2:
            printf((char *)arg0, arg1, arg2);
            break;
        default:
            printf("Warning: Invalid umaincall number %u!\n", type); //could not use printf in kernel
            break;
    }

    csr_write(0x8a4, dasics_return_pc);             // DasicsReturnPC
    csr_write(0x8a5, dasics_free_zone_return_pc);   // DasicsFreeZoneReturnPC

    return retval;
}

void dasics_ufault_handler(void)
{
    // Save some registers that should be saved by callees
    uint64_t dasics_return_pc = csr_read(0x8a4);
    uint64_t dasics_free_zone_return_pc = csr_read(0x8a5);

    uint64_t ucause = csr_read(ucause);
    uint64_t utval = csr_read(utval);
    uint64_t uepc = csr_read(uepc);

    printf("Info: ufault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx\n", ucause, uepc, utval);
    printf("Info: dasics_return_pc:0x%lx\n", dasics_return_pc);	

    // switch base on cause types.
    // currently just skip this inst.
    	csr_write(uepc, uepc + 4);
    // rvc will compress jump/branch inst.
    //if (ucause == EXC_DASICS_UFETCH_FAULT)
      //  csr_write(uepc, uepc + 2);
    //else 
       // csr_write(uepc, uepc + 4); 

    // Restore those saved registers
    csr_write(0x8a4, dasics_return_pc);
    csr_write(0x8a5, dasics_free_zone_return_pc);

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
