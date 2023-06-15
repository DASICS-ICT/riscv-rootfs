#ifndef _INCLUDE_DASICS_ECALL_H
#define _INCLUDE_DASICS_ECALL_H

#include <syscall_number.h>
#include <stdint.h>

/*
 * The ecall check struct
 */
typedef struct ecall_check
{
    int (*check)(unsigned long, \
                 unsigned long, \
                 unsigned long, \
                 unsigned long, \ 
                 unsigned long, \
                 unsigned long, \
                 unsigned long, \
                 unsigned long);
    int (*handle_error)();

} ecall_check_t;

extern ecall_check_t syscall_check[__NR_syscalls];

int default_ecall_check_handler(uint64_t a0, uint64_t a1, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7);
void default_ecall_error_handler();
void init_syscall_check();

#endif