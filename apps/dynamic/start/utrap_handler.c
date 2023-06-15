#include <utrap_handler.h>
#include <dasics_link.h>
#include <udasics.h>
#include <utrap.h>
#include <stdlib.h>
#include <syscall_number.h>
#include <dasics_ecall.h>
/*
 * This function is used to deal with the DasicsUFetchFault
 * error return -1
 */
int handle_DasicsUFetchFault(regs_context_t * r_regs)
{
    umain_got_t * _got_entry = _get_trap_area(r_regs->uepc);
// #ifdef DASICS_DEBUG
    my_printf("[ufault info]: hit fetch ufault uepc: 0x%lx hit ufault: 0x%lx, ulib func return: 0x%lx, dasicsReturnPC: 0x%lx, freeZoneReturnPC: 0x%lx\n\n\n", \
                r_regs->uepc, r_regs->ucause, r_regs->utval, r_regs->dasicsReturnPC, r_regs->dasicsFreeZoneReturnPC);
    // exit(1);
    // debug_print_umain_map(_got_entry);
// #endif
    // if (_got_entry == NULL)
    // {
    // #ifdef DASICS_DEBUG
    //     my_printf("[error]: failed to find the _got_entry\n");
    // #endif
    //     exit(1);
    // } 
    // judge the plt hit
    // int plt_jump = deal_jump_to_plt(_got_entry, r_regs);

    // if (plt_jump == HIT) return 0;
    // else if (plt_jump == HIT_ERROR) 
    // {
    // #ifdef DASICS_DEBUG
    //     my_printf("[error]: the plt call ulib function error\n");
    // #endif
    //     // while (1);
    //     exit(1);
    // }
    
    // just let it go
    if (r_regs->utval != r_regs->dasicsReturnPC)
    /* no nest default */
        r_regs->dasicsReturnPC = r_regs->utval;   
    else 
        r_regs->dasicsFreeZoneReturnPC = r_regs->utval;    
    return 0;
}

/*
 * This function is used to deal with the DasicsULoadFault
 * error return -1
 */
int handle_DasicsULoadFault(regs_context_t * r_regs)
{
    int idx = 0;
    umain_got_t * _got_entry = _get_trap_area(r_regs->uepc);
// #ifdef DASICS_DEBUG
    my_printf("[ufault info]: hit read ufault uepc: 0x%lx, address: 0x%lx \n", r_regs->uepc, r_regs->utval);
    // debug_print_umain_map(_got_entry);
// #endif
    if (_got_entry == NULL)
    {
    #ifdef DASICS_DEBUG
        my_printf("[error]: failed to find the _got_entry\n");
    #endif
        exit(1);
    }
    // judge the plt hit
    // int plt_result = deal_plt(_got_entry, r_regs);
    int plt_result = 0;

    if (plt_result == HIT) return 0;
    else if (plt_result == HIT_ERROR) 
    {
    #ifdef DASICS_DEBUG
        my_printf("[error]: the plt call ulib function error\n");
    #endif
        // while (1);
        exit(1);
    }
        
    // idx = trap_libcfg_alloc(r_regs, DASICS_LIBCFG_R, ROUND(r_regs->utval, 2 * PAGE_SIZE), ROUNDDOWN(r_regs->utval, 2 * PAGE_SIZE));
    r_regs->uepc += 4;
    if (idx == -1)
    {
        my_printf("[error]: no more libbounds!!\n");
        // while(1);
        exit(1);
    }    
    return 0;
}


/*
 * This function is used to deal with the DasicsULoadFault
 * error return -1
 */
int handle_DasicsUStoreFault(regs_context_t * r_regs)
{
    int idx = 0;
    umain_got_t * _got_entry = _get_trap_area(r_regs->uepc);
// #ifdef DASICS_DEBUG
    my_printf("[ufault info]: hit write ufault uepc: 0x%lx, address: 0x%lx \n", r_regs->uepc, r_regs->utval);
    // debug_print_umain_map(_got_entry);
// #endif
    if (_got_entry == NULL)
    {
    #ifdef DASICS_DEBUG
        my_printf("[error]: failed to find the _got_entry\n");
    #endif
        exit(1);
    }

    // idx = trap_libcfg_alloc(r_regs, DASICS_LIBCFG_W, ROUND(r_regs->utval, 2 * PAGE_SIZE), ROUNDDOWN(r_regs->utval, 2 * PAGE_SIZE));
    r_regs->uepc += 4;
    if (idx == -1)
    {
        my_printf("[error]: no more libbounds!!\n");
        exit(1);
    }    
    return 0;
}

static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
                           long arg3, long arg4, long arg5, long arg6)
{
    register long a7 asm("a7") = sysno;
    register long a0 asm("a0") = arg0;
    register long a1 asm("a1") = arg1;
    register long a2 asm("a2") = arg2;
    register long a3 asm("a3") = arg3;
    register long a4 asm("a4") = arg4;
    register long a5 asm("a5") = arg5;
    register long a6 asm("a6") = arg6;

    asm volatile("ecall"                      \
                 : "+r"(a0)                   \
                 : "r"(a1), "r"(a2), "r"(a3), \
                   "r"(a4), "r"(a5), "r"(a6), \
                   "r"(a7)                    \
                 : "memory");

    return a0;
}

/*
 * This function is used to deal with the DasicsUEcallFault
 * error return -1
 */
int handle_DasicsUEcallFault(regs_context_t * r_regs)
{
    umain_got_t * entry = _get_trap_area(r_regs->uepc);
    // jump ecall
    r_regs->uepc = r_regs->uepc + 4;

    // catch the specical syscall
    #ifdef DASICS_DEBUG
        if(r_regs->a7 == __NR_brk)
        {
            my_printf("[DEBUG]: Library %s want to call brk\n", entry->real_name);
        } else if (r_regs->a7 == __NR_mmap)
        {
            my_printf("[DEBUG]: Library %s want to call mmap\n", entry->real_name);
        } else if (r_regs->a7 == __NR_nanosleep)
        {
            my_printf("[DEBUG]: Library %s do syscall: sleep, ecall: %d\n", entry->real_name, r_regs->a7);
        } else if (r_regs->a7 == __NR_getpid)
        {
            my_printf("[DEBUG]: Library %s do syscall: getpid, ecall: %d\n", entry->real_name, r_regs->a7);
        } else if (r_regs->a7 == __NR_clock_nanosleep)
        {
            my_printf("[DEBUG]: Library %s do syscall: clock_nanosleep, ecall: %d\n", entry->real_name, r_regs->a7);
        } 
    #endif
    // check
    if (syscall_check[r_regs->a7].check(r_regs->a0, \
                                        r_regs->a1, \
                                        r_regs->a2, \
                                        r_regs->a3, \
                                        r_regs->a4, \
                                        r_regs->a5, \
                                        r_regs->a6, \
                                        r_regs->a7))
    {
        syscall_check[r_regs->a7].handle_error();
    }

    r_regs->a0 = invoke_syscall(r_regs->a7, \
                                r_regs->a0, \
                                r_regs->a1, \
                                r_regs->a2, \
                                r_regs->a3, \
                                r_regs->a4, \
                                r_regs->a5, \
                                r_regs->a6);
    return 0;
}