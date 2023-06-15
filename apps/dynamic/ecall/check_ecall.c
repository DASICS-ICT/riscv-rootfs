#include <dasics_ecall.h>
#include <my_stdio.h>
#include <stdint.h>

ecall_check_t syscall_check[__NR_syscalls];

/*  
 * default no check for the ecall 
 */
int default_ecall_check_handler(uint64_t a0, uint64_t a1, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7)
{
    #ifdef DASICS_DEBUG
        my_printf("[DEBUG]: catch user syscall %lx\n", a7);
    #endif
    return 0;    
}


/* default ecall error handler */
void default_ecall_error_handler()
{
    exit(1);
}

/* init all syscall's check handler */
void init_syscall_check()
{
    for (int i = 0; i < __NR_syscalls; i++)
    {
        syscall_check[i].check = default_ecall_check_handler;
        syscall_check[i].handle_error = default_ecall_error_handler;
    }
    
}