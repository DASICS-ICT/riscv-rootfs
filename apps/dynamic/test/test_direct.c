#include "test.h"
#include <my_string.h>

/* This is used to test lib func */
void test_lib_memcpy()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [my_memcpy] test\n");
    register long sp_now asm("sp");
    uint64_t brk = BRK(0);
    BRK(brk + 60);
    my_printf("brk bound lo: 0x%lx, brk bound hi: 0x%lx\n", brk, brk + 10);  
#ifdef DASICS_LINUX
    // alloc stack
    int32_t idx_1 =  dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R, sp_now, sp_now - PAGE_SIZE);
    // limit the heap
    int32_t idx_2 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W, brk + 10, brk);
    // limit the read bound    
    int32_t idx_3 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R, (uint64_t)test_str + 34, (uint64_t)test_str);
#endif
    my_memcpy((void *)brk, test_str, 34);

    BRK(brk);
    my_printf("[TEST END]: mem write protect [my_memcpy] test end\n");
    clear_libcfg();
}

void test_lib_strcpy()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [my_strcpy] test\n");
    register long sp_now asm("sp");
    uint64_t brk = BRK(0);
    BRK(brk + 60);
    my_printf("brk bound lo: 0x%lx, brk bound hi: 0x%lx\n", brk, brk + 10);  
#ifdef DASICS_LINUX
    // alloc stack
    int32_t idx_1 =  dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R, sp_now, sp_now - PAGE_SIZE);
    // limit the heap
    int32_t idx_2 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W, brk + 10, brk);
    // limit the read bound    
    int32_t idx_3 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R, (uint64_t)test_str + 34, (uint64_t)test_str);
#endif
    my_strcpy((char *)brk, test_str);

    BRK(brk);
    my_printf("[TEST END]: mem write protect [my_strcpy] test end\n");
    clear_libcfg();
}

void test_lib_memset()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [my_memset] test\n");
    register long sp_now asm("sp");
    uint64_t brk = BRK(0);
    BRK(brk + 60);
    my_printf("brk bound lo: 0x%lx, brk bound hi: 0x%lx\n", brk, brk + 10);  
#ifdef DASICS_LINUX
    // alloc stack
    int32_t idx_1 =  dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R, sp_now, sp_now - PAGE_SIZE);
    // limit the heap
    int32_t idx_2 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W, brk + 10, brk);
#endif
    my_memset((void *)brk, 0, 60);

    BRK(brk);
    my_printf("[TEST END]: mem write protect [my_memset] test end\n");
    clear_libcfg();

}

void test_lib_bzero()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [my_bzero] test\n");
    register long sp_now asm("sp");
    uint64_t brk = BRK(0);
    BRK(brk + 60);
    my_printf("brk bound lo: 0x%lx, brk bound hi: 0x%lx\n", brk, brk + 10);  
#ifdef DASICS_LINUX
    // alloc stack
    int32_t idx_1 =  dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R, sp_now, sp_now - PAGE_SIZE);
    // limit the heap
    int32_t idx_2 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W, brk + 10, brk);
#endif
    my_bzero((void *)brk, 60);

    BRK(brk);
    my_printf("[TEST END]: mem write protect [my_bzero] test end\n");
    clear_libcfg();
}

void test_lib_strcmp()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [my_strcmp] test\n");
    register long sp_now asm("sp");
    uint64_t brk = BRK(0);
    BRK(brk + 60);
    my_printf("brk bound lo: 0x%lx, brk bound hi: 0x%lx\n", brk, brk + 10);  
#ifdef DASICS_LINUX
    // alloc stack
    int32_t idx_1 =  dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R, sp_now, sp_now - PAGE_SIZE);
    // limit the heap
    int32_t idx_2 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R, brk + 10, brk);
    // limit the read bound  
    int32_t idx_3 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R, (uint64_t)test_str + 34, (uint64_t)test_str);     
#endif
    int i;
    for (i = 0; test_str[i] != '\0'; i++)
    {
        ((char *)brk)[i] = test_str[i];
    }
    ((char *)brk)[i] = '\0';
    
    my_strcmp((char *)brk, test_str);

    BRK(brk);
    my_printf("[TEST END]: mem write protect [my_strcmp] test end\n");
    clear_libcfg();
}

void test_lib_strncmp()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [my_strncmp] test\n");
    register long sp_now asm("sp");
    uint64_t brk = BRK(0);
    BRK(brk + 60);
    my_printf("brk bound lo: 0x%lx, brk bound hi: 0x%lx\n", brk, brk + 10);  
#ifdef DASICS_LINUX
    // alloc stack
    int32_t idx_1 =  dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R, sp_now, sp_now - PAGE_SIZE);
    // limit the heap
    int32_t idx_2 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R, brk + 10, brk);
    // limit the read bound  
    int32_t idx_3 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R, (uint64_t)test_str + 34, (uint64_t)test_str);      
#endif
    int i;
    for (i = 0; test_str[i] != '\0'; i++)
    {
        ((char *)brk)[i] = test_str[i];
    }
    ((char *)brk)[i] = '\0';
    
    my_strncmp((void *)brk, test_str, i);

    BRK(brk);
    my_printf("[TEST END]: mem write protect [my_strncmp] test end\n");
    clear_libcfg();
}

void test_lib_strncpy()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [my_strncpy] test\n");
    register long sp_now asm("sp");
    uint64_t brk = BRK(0);
    BRK(brk + 60);
    my_printf("brk bound lo: 0x%lx, brk bound hi: 0x%lx\n", brk, brk + 10);  
#ifdef DASICS_LINUX
    // alloc stack
    int32_t idx_1 =  dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R, sp_now, sp_now - PAGE_SIZE);
    // limit the heap
    int32_t idx_2 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W, brk + 10, brk);
    // limit the read bound    
    int32_t idx_3 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R, (uint64_t)test_str + 34, (uint64_t)test_str);      

#endif
    int i;
    for (i = 0; test_str[i] != '\0'; i++);
    
    my_strncpy((void *)brk, test_str, i);

    BRK(brk);
    my_printf("[TEST END]: mem write protect [my_strncpy] test end\n");
    clear_libcfg();
}

void test_lib_strcat()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [my_strcat] test\n");
    register long sp_now asm("sp");
    uint64_t brk = BRK(0);
    BRK(brk + 60);
    my_printf("brk bound lo: 0x%lx, brk bound hi: 0x%lx\n", brk, brk + 36);  
#ifdef DASICS_LINUX
    // alloc stack
    int32_t idx_1 =  dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R, sp_now, sp_now - PAGE_SIZE);
    // limit the heap
    int32_t idx_2 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R | DASICS_LIBCFG_W, brk + 36, brk);
    // limit the read bound    
    int32_t idx_3 = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R, (uint64_t)test_str + 34, (uint64_t)test_str);      
#endif
    int i;
    for (i = 0; test_str[i] != '\0'; i++)
    {
        ((char *)brk)[i] = test_str[i];
    }
    ((char *)brk)[i] = '\0';
    
    
    char * cat_str = "hello world\n";
    my_printf("cat_str: 0x%lx\n", cat_str);
    my_strcat((void *)brk, cat_str);

    BRK(brk);
    my_printf("[TEST END]: mem write protect [my_strcat] test end\n");
    clear_libcfg();
}