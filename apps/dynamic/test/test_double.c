#include "test.h"
#include <my__string.h>

/* This is used to test nested call */
void test_lib__memcpy()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [my__memcpy] test\n");
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
    my__memcpy((void *)brk, test_str, 34);

    BRK(brk);
    my_printf("[TEST END]: mem write protect [my__memcpy] test end\n");
    clear_libcfg();
}

void test_lib__strcpy()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [my__strcpy] test\n");
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
    my__strcpy((char *)brk, test_str);

    BRK(brk);
    my_printf("[TEST END]: mem write protect [my__strcpy] test end\n");
    clear_libcfg();
}

void test_lib__memset()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [my__memset] test\n");
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
    my__memset((void *)brk, 0, 60);

    BRK(brk);
    my_printf("[TEST END]: mem write protect [my__memset] test end\n");
    clear_libcfg();
}

void test_lib__bzero()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [my__bzero] test\n");
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
    // my__bzero((void *)brk, 60);

    BRK(brk);
    my_printf("[TEST END]: mem write protect [my__bzero] test end\n");
    clear_libcfg();
}

void test_lib__strcmp()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [my__strcmp] test\n");
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
    
    my__strcmp((char *)brk, test_str);

    BRK(brk);
    my_printf("[TEST END]: mem write protect [my__strcmp] test end\n");
    clear_libcfg();
}

void test_lib__strncmp()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [my__strncmp] test\n");
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
    
    my__strncmp((void *)brk, test_str, i);

    BRK(brk);
    my_printf("[TEST END]: mem write protect [my__strncmp] test end\n");
    clear_libcfg();
}

void test_lib__strncpy()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [my__strncpy] test\n");
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
    
    my__strncpy((void *)brk, test_str, i);

    BRK(brk);
    my_printf("[TEST END]: mem write protect [my__strncpy] test end\n");
    clear_libcfg();

}

void test_lib__strcat()
{
    clear_libcfg();
    my_printf("\n\n[TEST START]: mem write protect [my__strcat] test\n");
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
    my__strcat((void *)brk, cat_str);

    BRK(brk);
    my_printf("[TEST END]: mem write protect [my__strcat] test end\n");
    clear_libcfg();    
}