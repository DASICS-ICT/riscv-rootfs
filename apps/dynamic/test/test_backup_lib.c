#include "test.h"
#include <my_string.h>
#include <my_stdio.h>
#include <stdlib.h>
#include <stdio.h>

void test_lib_sleep()
{

    clear_libcfg(); 

#ifdef DASICS_LINUX
    register long sp_now asm("sp");
    // alloc stack
    int32_t idx_x =  dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R, sp_now, sp_now - PAGE_SIZE);
#endif

    my_printf("\n\n[TEST START]: backup lib sleep\n");
    my_printf("\ttest func [sleep]\n");

    // main call sleep
    my_printf("\t [main call sleep]\n");
    sleep(1);

    // mylib.so call sleep
    my_printf("\t [lib call sleep]\n");    
    my_sleep(2);


    my_printf("[TEST END]: backup lib end\n");

    clear_libcfg(); 
}

void test_lib_getpid()
{

    clear_libcfg(); 

#ifdef DASICS_LINUX
    register long sp_now asm("sp");
    // alloc stack
    int32_t idx_x =  dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R, sp_now, sp_now - PAGE_SIZE);
#endif

    my_printf("\n\n[TEST START]: backup lib getpid\n");
    my_printf("\ttest func [getpid]\n");

    // main call getpid
    my_printf("\t [main call getpid]\n");
    my_printf("\t  main call pid: %d\n", getpid());

    // mylib.so call getpid
    my_printf("\t [lib call getpid]\n");    
    my_printf("\t  lib call pid: %d\n", my_getpid());



    my_printf("[TEST END]: backup lib end\n");

    clear_libcfg(); 
}


void test_lib_malloc()
{
    clear_libcfg(); 
#ifdef DASICS_LINUX
    register long sp_now asm("sp");
    // alloc stack
    int32_t idx_x =  dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R, sp_now, sp_now - PAGE_SIZE);
#endif
    my_printf("\n\n[TEST START]: backup lib test\n");
    my_printf("\ttest func [malloc]\n");

    // main call malloc
    my_printf("\t [main call malloc]\n");
    char * buff = (char *)malloc(16);
    char * str = "\tHello world\n";
    if (buff == NULL)
    {
        my_printf("\t main call malloc error\n");    
        goto error;
    }
    for (int i = 0; i < 14; i++)
    {
        buff[i] = str[i];
    }
    my_printf("\taddr: 0x%lx, str: %s",(uint64_t)buff, buff);
    free(buff);

    // mylib.so call malloc
    my_printf("\t [lib call malloc]\n");    
    buff = (char *)my_malloc(16);
    if (buff == NULL)
    {
        my_printf("\t lib call malloc error\n");    
        goto error;
    }
    for (int i = 0; i < 14; i++)
    {
        buff[i] = str[i];
    }
    my_printf("\taddr: 0x%lx, str: %s",(uint64_t)buff, buff);
    // my_free(buff);    

error:
    my_printf("[TEST END]: backup lib end\n");

    clear_libcfg(); 
}


void test_lib_read()
{
    clear_libcfg(); 
#ifdef DASICS_LINUX
    register long sp_now asm("sp");
    // alloc stack
    int32_t idx_x =  dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R, sp_now, sp_now - PAGE_SIZE);
#endif
    my_printf("\n\n[TEST START]: backup lib test\n");
    my_printf("\ttest func [read]\n");

    FILE *test = fopen("cert.crt", "r");
    fseek(test, 0, SEEK_SET);

    // main call malloc
    char * buff = (char *)malloc(256);
    if (buff == NULL)
    {
        my_printf("\t main call malloc error\n");    
        goto error;
    }

    my_printf("\t [main call read]\n");
    fread(buff, 1, 256, test);
    for (int i = 0; i < 256; i++)
    {
        printf("%c", buff[i]);
    }
    printf("\n");
    free(buff);

    // mylib.so call malloc
    my_printf("\t [lib call read]\n");    
    my_read();
    // my_free(buff);    

error:
    my_printf("[TEST END]: backup lib end\n");
    fclose(test);
    clear_libcfg();     
}