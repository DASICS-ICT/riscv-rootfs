#ifndef _TEST_TEST_H
#define _TEST_TEST_H

#include <stdint.h>
#include <stdlib.h>
#include <utrap.h>
#include <udasics.h>
#include <my_stdio.h>

extern char * test_str;
/* this is used to test trusted lib and untrusted lib */
void test_lib_sleep();
void test_lib_getpid();
void test_lib_malloc();
void test_lib_read();

/* This is used to test lib func */
void test_lib_memcpy();
void test_lib_strcpy();
void test_lib_memset();
void test_lib_bzero();
void test_lib_strcmp();
void test_lib_strncmp();
void test_lib_strncpy();
void test_lib_strcat();

/* This is used to test nested call */
void test_lib__memcpy();
void test_lib__strcpy();
void test_lib__memset();
void test_lib__bzero();
void test_lib__strcmp();
void test_lib__strncmp();
void test_lib__strncpy();
void test_lib__strcat();

/* for thesis */
void test_thesis_memcpy_read();
void test_thesis_memcpy_write();
void test_thesis_jmp();
void test_self_heap();

static void dump_all_bound()
{
    my_printf("DLCFG0: 0x%lx, DLCFG1: 0x%lx\n", csr_read(0x881), csr_read(0x882));
    my_printf("debug hi: 0x%lx, lo: 0x%lx\n", csr_read(0x883), csr_read(0x884));
    my_printf("debug hi: 0x%lx, lo: 0x%lx\n", csr_read(0x885), csr_read(0x886));
    my_printf("debug hi: 0x%lx, lo: 0x%lx\n", csr_read(0x887), csr_read(0x888));
    my_printf("debug hi: 0x%lx, lo: 0x%lx\n", csr_read(0x889), csr_read(0x88a));
    my_printf("debug hi: 0x%lx, lo: 0x%lx\n", csr_read(0x88b), csr_read(0x88c));
    my_printf("debug hi: 0x%lx, lo: 0x%lx\n", csr_read(0x88d), csr_read(0x88e));
    my_printf("debug hi: 0x%lx, lo: 0x%lx\n", csr_read(0x88f), csr_read(0x890));
    my_printf("debug hi: 0x%lx, lo: 0x%lx\n", csr_read(0x891), csr_read(0x892));
    my_printf("debug hi: 0x%lx, lo: 0x%lx\n", csr_read(0x893), csr_read(0x894));
    my_printf("debug hi: 0x%lx, lo: 0x%lx\n", csr_read(0x895), csr_read(0x896));
    my_printf("debug hi: 0x%lx, lo: 0x%lx\n", csr_read(0x897), csr_read(0x898));
    my_printf("debug hi: 0x%lx, lo: 0x%lx\n", csr_read(0x899), csr_read(0x89a));
    my_printf("debug hi: 0x%lx, lo: 0x%lx\n", csr_read(0x89b), csr_read(0x89c));
    my_printf("debug hi: 0x%lx, lo: 0x%lx\n", csr_read(0x89d), csr_read(0x89e));
    my_printf("debug hi: 0x%lx, lo: 0x%lx\n", csr_read(0x89f), csr_read(0x8a0));
    my_printf("debug hi: 0x%lx, lo: 0x%lx\n", csr_read(0x8a1), csr_read(0x8a2));
}

static inline uint64_t BRK(uint64_t ptr)
{
    register long a7 asm("a7") = 214;
    register long a0 asm("a0") = ptr;
    asm volatile("ecall"                        \
                 : "+r"(a0)                     \
                 : "r"(a7)                      \
                 : "memory");

    return a0;
}

static void clear_libcfg()
{
#ifdef DASICS_LINUX
    for (int i = 3; i < 2 * DASICS_LIBCFG_WIDTH; i++)
    {
        dasics_libcfg_free(i);
    }
#endif       
}

#endif