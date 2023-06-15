#include "test.h"
#include <my_string.h>

void test_thesis_memcpy_read()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem read protect [my_memcpy] test\n");
    register long sp_now asm("sp");
    uint64_t brk = BRK(0);

    BRK(brk + 60);
    for (int i = 0; i < 60; i++)
    {
        /* code */
        *((char *)(brk + i)) = 0;
    }
    
    char * source_data = "wh12345";
    my_printf("source_data address: 0x%lx\n", source_data);
    my_printf("source_data: {%s}\n", source_data);
    my_printf("buffer address: 0x%lx\n", brk);
    my_printf("buffer: {%s}\n\n\n", (char *)brk);
    // my_printf("brk bound lo: 0x%lx, brk bound hi: 0x%lx\n", brk, brk + 10);  
#ifdef DASICS_LINUX
    // limit the heap
    int32_t idx_1 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W, brk + 7, brk);
    // limit the read bound    
    int32_t idx_2 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R, (uint64_t)source_data + 1, (uint64_t)source_data);
#endif
    my_printf("call my__memcpy\n");
    my__memcpy((void *)brk, source_data, 7);

    my_printf("source_data: {%s}\n", source_data);
    my_printf("buffer: {%s}\n\n\n", (char *)brk);    

    BRK(brk);
    my_printf("[TEST END]: mem read protect [my_memcpy] test end\n");
    clear_libcfg();
}

void test_thesis_memcpy_write()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [my_memcpy] test\n");
    register long sp_now asm("sp");
    uint64_t brk = BRK(0);

    BRK(brk + 60);
    for (int i = 0; i < 60; i++)
    {
        /* code */
        *((char *)(brk + i)) = 0;
    }
    
    char * source_data = "wh12345";
    my_printf("source_data address: 0x%lx\n", source_data);
    my_printf("source_data: {%s}\n", source_data);
    my_printf("buffer address: 0x%lx\n", brk);
    my_printf("buffer: {%s}\n\n\n", (char *)brk);
    // my_printf("brk bound lo: 0x%lx, brk bound hi: 0x%lx\n", brk, brk + 10);  
#ifdef DASICS_LINUX
    // limit the heap
    int32_t idx_1 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W, brk + 1, brk);
    // limit the read bound    
    int32_t idx_2 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R, (uint64_t)source_data + 7, (uint64_t)source_data);
#endif
    my_printf("call my_memcpy\n");
    my_memcpy((void *)brk, source_data, 7);

    my_printf("source_data: {%s}\n", source_data);
    my_printf("buffer: {%s}\n\n\n", (char *)brk);    

    BRK(brk);
    my_printf("[TEST END]: mem write protect [my_memcpy] test end\n");
    clear_libcfg();

}

void test_thesis_jmp()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [lib_call_rand] test\n\n");

    lib_call_rand();

    my_printf("[TEST END]: mem write protect [lib_call_rand] test end\n\n");
    clear_libcfg();

}


void test_cross_lib()
{

    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [lib_call_rand] test\n\n");

    cross_lib_test();

    my_printf("[TEST END]: mem write protect [lib_call_rand] test end\n\n");
    clear_libcfg();

}

void test_self_heap()
{
    my_printf("\n\n[TEST START]: self-management heap test\n\n");

    my_printf("malloc str\n");

    char * str = (char *)my_malloc(12);

    char * _str = "Hello world\n";

    for (int i = 0; i < 13; i++)
    {
        str[i] = _str[i];
    }
    
    my_printf("str's address 0x%lx\n", str);
    my_printf("str: %s\n\n", str);
    
    my_printf("realloc str: %s\n\n", str);
    str = (char *)my_realloc(str, 20);

    my_printf("str's address 0x%lx\n", str);
    my_printf("str: %s\n\n", str);

    my_printf("free str\n");
    my_free(str);

    my_printf("\n\n[TEST END]: self-management heap test end\n\n");

}