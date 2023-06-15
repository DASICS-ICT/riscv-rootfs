#include <asm/asm.h>

ENTRY(_set_utvec)

#ifdef DASICS_LINUX
    li a0, 1
    la a1, msg 
    li a2, 17
    li a7, 64
    ecall
    /* Load the global pointer*/  
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop  
    
    mv a0, sp
    call _dasics_entry_stage1
    li a0, 0
#endif
    call _start
END(_set_utvec)


ENTRY(_umain_entry)
    /* Load the global pointer*/  
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop  

    /* 
     * we must save a0 for the func 
     * before we enter the _start
     */
    addi sp, sp, -8
    sd a0, (sp)
    mv a1, a0
    addi a0, sp, 8
    call _dasics_entry_stage2
    ld a0, (sp)
    addi sp, sp, 8
    call _start
END(_umain_entry)

ENTRY(_copy_lib_entry)
    /* Load the global pointer*/  
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop  

    /* 
     * we must save a0 for the func 
     * before we enter the _start
     */
    addi sp, sp, -8
    sd a0, (sp)
    mv a1, a0
    addi a0, sp, 8
    call _dasics_entry_stage3
    ld a0, (sp)
    addi sp, sp, 8
    la a0, dll_fini
    ld a0, (a0)
    call _start
END(_copy_lib_entry)


.section .data 
.local msg   
msg: .string "let's begin...\n\r"