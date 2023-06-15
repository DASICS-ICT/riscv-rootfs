#include <stdio.h>
#include <uattr.h>
#include <dasics_link.h>
#include "test.h"


static void foo(void) __attribute__ ((constructor));

void exit_function() {
	printf("[Finish] test dasics finished\n");
}

void foo(void)
{
    printf("[Constructor] I am a Constructor function\n");
}

char * test_str = "I'm a string length more than 10\n";


int main(int argc, char *argv[])
{
    clear_libcfg();
    printf("hello riscv!\n");
    atexit(exit_function);
    /* 
     * open area for lib func exit, it must be execute fist when 
     * execute fini of library
     */ 
    atexit(ready_lib_exit);

    for (int i = 0; i < argc; i++)
    {
        printf("argc[%d]: %s\n",i + 1, argv[i]);
    }


    // Test backup lib
    test_lib_sleep();
    test_lib_getpid();   
    test_lib_malloc();
    test_lib_malloc();
    test_lib_read();

    // Test lib call protect
    test_lib_memcpy();
    test_lib_strcpy();
    test_lib_memset();
    test_lib_bzero();
    test_lib_strcmp();
    test_lib_strncmp();
    test_lib_strncpy();
    test_lib_strcat();

    // Test jump lib call
    test_lib__memcpy();
    test_lib__strcpy();
    test_lib__memset();
    test_lib__bzero();
    test_lib__strcmp();
    test_lib__strncmp();
    test_lib__strncpy();
    test_lib__strcat();    

    // test for self manageheap
    test_self_heap();


    // test for wanghan's graduation projection
    test_thesis_memcpy_read();
    test_thesis_memcpy_write();
    test_thesis_jmp();
    

    printf("over\n");
    return 0;
}
