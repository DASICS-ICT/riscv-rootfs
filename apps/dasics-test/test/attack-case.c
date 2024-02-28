#include <stdio.h>
#include <udasics.h>
#include <stdlib.h>
#include <string.h>
#include <uattr.h>

void rop_target();
void ATTR_UFREEZONE_TEXT Malicious(void * unsed, const void * src, size_t length);
void ATTR_UFREEZONE_TEXT homebrew_memcpy(void * dst, const void * src, size_t length);

#define OLD_BP_PTR   __builtin_frame_address(0)
#define RET_ADDR_PTR ((void **) OLD_BP_PTR - 1)

uint64_t stack_buffer_handler;
uint64_t stack_handler;

static char data_secret[32] = "success. Secret data leaked.\n";


int main(int argc, char * argv[])
{
    csr_write(0x880, 0);
    register_udasics(0);
    // Save secret data
    char stack_secret_data[32];
    strcpy(stack_secret_data, data_secret);

    // Buffer send to Malicious
    char stack_buffer[1024];

    printf("> [Begin] Call Malicious\n");

    stack_buffer_handler =  dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R, \
                                                        (uint64_t)stack_buffer, \
                                                        (uint64_t)stack_buffer + 1024 -1 );

    register uint64_t sp asm("sp");
    stack_handler = dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_W | DASICS_LIBCFG_R, \
                                                        sp - 0x2000, \
                                                        sp);
    lib_call_arg(&Malicious, (uint64_t)stack_buffer, 1024, 0);

    printf("> [End] Back to Main\n");

    dasics_libcfg_free(stack_buffer_handler);
    dasics_libcfg_free(stack_handler);
    
    unregister_udasics();
    return 0;
}

void
rop_target()
{
    printf("success.\nROP function reached.\n");
    exit(0);
}

void ATTR_UFREEZONE_TEXT 
Malicious(void * unsed, const void * src, size_t length)
{
    // Local buffer on stack
    char private_buffer[1024 + 32 + 16];

    // Copy main's stack_buffer, stack_secret_data, ret_address
    homebrew_memcpy(private_buffer, src, length + 32 + 16);

    char * secret = &private_buffer[length];

    uint64_t * ret_addr = (uint64_t *)(&private_buffer[length + 32 + 8]);

    dasics_umaincall(Umaincall_PRINT, " - - - - - - Malicious Call - - - - - - - -  \n");   


    dasics_umaincall(Umaincall_PRINT, " Try to get Secret data: %s, ret point: 0x%lx, ret_addr: 0x%lx \n", \
                     secret, \
                     &((char *)src)[length + 32 + 8], \
                     *ret_addr \
                     );    
    dasics_umaincall(Umaincall_PRINT, " Try to change return address of main to Rop: 0x%lx \n", \
                     (uint64_t)&rop_target + 8
                     );    

    // Try to change ret address and do Rop attack
    ((uint64_t* )src)[(length + 32 + 8) / 8] = (uint64_t)&rop_target + 8;

    dasics_umaincall(Umaincall_PRINT, " - - - - - - Malicious Call End- - - - - - - -  \n");    

    return;

}



void ATTR_UFREEZONE_TEXT
homebrew_memcpy(void * dst, const void * src, size_t length)
{
    char * d, * s;

    d = (char *) dst;
    s = (char *) src;

    while (length--) {
        *d++ = *s++;
    }
}
