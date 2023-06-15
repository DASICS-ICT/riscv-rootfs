#include <asm/regs.h>
#include <asm/ucsr.h>
#include <asm/asm.h>
.macro SAVE_CONTEXT
    addi sp, sp, -(OFFSET_SIZE)
    /* save all general purpose registers here! */
    // return address
    sd ra, OFFSET_REG_RA(sp)

    // pointers, sp and tp will be handled later
    sd gp, OFFSET_REG_GP(sp)
    sd tp, OFFSET_REG_TP(sp)

    // temporary
    sd t0, OFFSET_REG_T0(sp)
    sd t1, OFFSET_REG_T1(sp)
    sd t2, OFFSET_REG_T2(sp)

    // saved register
    sd s0, OFFSET_REG_S0(sp)
    sd s1, OFFSET_REG_S1(sp)

    // args
    sd a0, OFFSET_REG_A0(sp)
    sd a1, OFFSET_REG_A1(sp)
    sd a2, OFFSET_REG_A2(sp)
    sd a3, OFFSET_REG_A3(sp)
    sd a4, OFFSET_REG_A4(sp)
    sd a5, OFFSET_REG_A5(sp)
    sd a6, OFFSET_REG_A6(sp)
    sd a7, OFFSET_REG_A7(sp)

    // saved register
    sd s2, OFFSET_REG_S2(sp)
    sd s3, OFFSET_REG_S3(sp)
    sd s4, OFFSET_REG_S4(sp)
    sd s5, OFFSET_REG_S5(sp)
    sd s6, OFFSET_REG_S6(sp)
    sd s7, OFFSET_REG_S7(sp)
    sd s8, OFFSET_REG_S8(sp)
    sd s9, OFFSET_REG_S9(sp)
    sd s10, OFFSET_REG_S10(sp)
    sd s11, OFFSET_REG_S11(sp)

    // temporary register
    sd t3, OFFSET_REG_T3(sp)
    sd t4, OFFSET_REG_T4(sp)
    sd t5, OFFSET_REG_T5(sp)
    sd t6, OFFSET_REG_T6(sp)    

    addi sp, sp, -(OFFSET_N_EXTENSION)

//    csrr t0, CSR_USTATUS
//    sd t0, OFFSET_REG_USTATUS(sp)

    csrr t0, CSR_UIE
    sd t0, OFFSET_REG_UIE(sp)

    csrr t0, CSR_UTVEC
    sd t0, OFFSET_REG_UTVEC(sp)

    csrr t0, CSR_USCRATCH
    sd t0, OFFSET_REG_USCRATCH(sp)

    csrr t0, CSR_UEPC
    sd t0, OFFSET_REG_UEPC(sp)

    csrr t0, CSR_UCAUSE
    sd t0, OFFSET_REG_UCAUSE(sp)

    csrr t0, CSR_UTVAL
    sd t0, OFFSET_REG_UBADADDR(sp)

    csrr t0, CSR_UIP
    sd t0, OFFSET_REG_UIP(sp)       

    addi sp, sp, -(OFFSET_DASICS)

    csrr t0, CSR_DLCFG0
    sd t0, OFFSET_DASICSLIBCFG0(sp)

    csrr t0, CSR_DLCFG1
    sd t0,OFFSET_DASICSLIBCFG1(sp)

    csrr t0, CSR_DLBOUND0
    sd t0, OFFSET_DASICSLIBBOUNDS0(sp)

    csrr t0, CSR_DLBOUND1
    sd t0, OFFSET_DASICSLIBBOUNDS1(sp)

    csrr t0, CSR_DLBOUND2
    sd t0, OFFSET_DASICSLIBBOUNDS2(sp)

    csrr t0, CSR_DLBOUND3
    sd t0, OFFSET_DASICSLIBBOUNDS3(sp)

    csrr t0, CSR_DLBOUND4
    sd t0, OFFSET_DASICSLIBBOUNDS4(sp)

    csrr t0, CSR_DLBOUND5
    sd t0, OFFSET_DASICSLIBBOUNDS5(sp)

    csrr t0, CSR_DLBOUND6
    sd t0, OFFSET_DASICSLIBBOUNDS6(sp)

    csrr t0, CSR_DLBOUND7
    sd t0, OFFSET_DASICSLIBBOUNDS7(sp)

    csrr t0, CSR_DLBOUND8
    sd t0, OFFSET_DASICSLIBBOUNDS8(sp)

    csrr t0, CSR_DLBOUND9
    sd t0, OFFSET_DASICSLIBBOUNDS9(sp)

    csrr t0, CSR_DLBOUND10
    sd t0, OFFSET_DASICSLIBBOUNDS10(sp)

    csrr t0, CSR_DLBOUND11
    sd t0, OFFSET_DASICSLIBBOUNDS11(sp)

    csrr t0, CSR_DLBOUND12
    sd t0, OFFSET_DASICSLIBBOUNDS12(sp)

    csrr t0, CSR_DLBOUND13
    sd t0, OFFSET_DASICSLIBBOUNDS13(sp)

    csrr t0, CSR_DLBOUND14
    sd t0, OFFSET_DASICSLIBBOUNDS14(sp)

    csrr t0, CSR_DLBOUND15
    sd t0, OFFSET_DASICSLIBBOUNDS15(sp)

    csrr t0, CSR_DLBOUND16
    sd t0, OFFSET_DASICSLIBBOUNDS16(sp)

    csrr t0, CSR_DLBOUND17
    sd t0, OFFSET_DASICSLIBBOUNDS17(sp)

    csrr t0, CSR_DLBOUND18
    sd t0, OFFSET_DASICSLIBBOUNDS18(sp)

    csrr t0, CSR_DLBOUND19
    sd t0, OFFSET_DASICSLIBBOUNDS19(sp)

    csrr t0, CSR_DLBOUND20
    sd t0, OFFSET_DASICSLIBBOUNDS20(sp)

    csrr t0, CSR_DLBOUND21
    sd t0, OFFSET_DASICSLIBBOUNDS21(sp)

    csrr t0, CSR_DLBOUND22
    sd t0, OFFSET_DASICSLIBBOUNDS22(sp)

    csrr t0, CSR_DLBOUND23
    sd t0, OFFSET_DASICSLIBBOUNDS23(sp)

    csrr t0, CSR_DLBOUND24
    sd t0, OFFSET_DASICSLIBBOUNDS24(sp)

    csrr t0, CSR_DLBOUND25
    sd t0, OFFSET_DASICSLIBBOUNDS25(sp)

    csrr t0, CSR_DLBOUND26
    sd t0, OFFSET_DASICSLIBBOUNDS26(sp)

    csrr t0, CSR_DLBOUND27
    sd t0, OFFSET_DASICSLIBBOUNDS27(sp)

    csrr t0, CSR_DLBOUND28
    sd t0, OFFSET_DASICSLIBBOUNDS28(sp)

    csrr t0, CSR_DLBOUND29
    sd t0, OFFSET_DASICSLIBBOUNDS29(sp)

    csrr t0, CSR_DLBOUND30
    sd t0, OFFSET_DASICSLIBBOUNDS30(sp)

    csrr t0, CSR_DLBOUND31
    sd t0, OFFSET_DASICSLIBBOUNDS31(sp)

    csrr t0, CSR_DMAINCALL
    sd t0, OFFSET_DASICSMAINCALLENTRY(sp)

    csrr t0, CSR_DRETURNPC
    sd t0, OFFSET_DASICSRETURNPC(sp)

    csrr t0, CSR_DFZRETURN
    sd t0, OFFSET_DASICSFREEZONERETURNPC(sp)     

.endm

.macro RESTORE_CONTEXT

    ld t0, OFFSET_DASICSLIBCFG0(sp)
    csrw CSR_DLCFG0, t0

    ld t0, OFFSET_DASICSLIBCFG1(sp)
    csrw CSR_DLCFG1, t0

    ld t0, OFFSET_DASICSLIBBOUNDS0(sp)
    csrw CSR_DLBOUND0, t0

    ld t0, OFFSET_DASICSLIBBOUNDS1(sp)
    csrw CSR_DLBOUND1, t0

    ld t0, OFFSET_DASICSLIBBOUNDS2(sp)
    csrw CSR_DLBOUND2, t0

    ld t0, OFFSET_DASICSLIBBOUNDS3(sp)
    csrw CSR_DLBOUND3, t0

    ld t0, OFFSET_DASICSLIBBOUNDS4(sp)
    csrw CSR_DLBOUND4, t0

    ld t0, OFFSET_DASICSLIBBOUNDS5(sp)
    csrw CSR_DLBOUND5, t0

    ld t0, OFFSET_DASICSLIBBOUNDS6(sp)
    csrw CSR_DLBOUND6, t0

    ld t0, OFFSET_DASICSLIBBOUNDS7(sp)
    csrw CSR_DLBOUND7, t0

    ld t0, OFFSET_DASICSLIBBOUNDS8(sp)
    csrw CSR_DLBOUND8, t0

    ld t0, OFFSET_DASICSLIBBOUNDS9(sp)
    csrw CSR_DLBOUND9, t0

    ld t0, OFFSET_DASICSLIBBOUNDS10(sp)
    csrw CSR_DLBOUND10, t0

    ld t0, OFFSET_DASICSLIBBOUNDS11(sp)
    csrw CSR_DLBOUND11, t0

    ld t0, OFFSET_DASICSLIBBOUNDS12(sp)
    csrw CSR_DLBOUND12, t0
    
    ld t0, OFFSET_DASICSLIBBOUNDS13(sp)
    csrw CSR_DLBOUND13, t0

    ld t0, OFFSET_DASICSLIBBOUNDS14(sp)
    csrw CSR_DLBOUND14, t0

    ld t0, OFFSET_DASICSLIBBOUNDS15(sp)
    csrw CSR_DLBOUND15, t0

    ld t0, OFFSET_DASICSLIBBOUNDS16(sp)
    csrw CSR_DLBOUND16, t0

    ld t0, OFFSET_DASICSLIBBOUNDS17(sp)
    csrw CSR_DLBOUND17, t0

    ld t0, OFFSET_DASICSLIBBOUNDS18(sp)
    csrw CSR_DLBOUND18, t0

    ld t0, OFFSET_DASICSLIBBOUNDS19(sp)
    csrw CSR_DLBOUND19, t0

    ld t0, OFFSET_DASICSLIBBOUNDS20(sp)
    csrw CSR_DLBOUND20, t0

    ld t0, OFFSET_DASICSLIBBOUNDS21(sp)
    csrw CSR_DLBOUND21, t0

    ld t0, OFFSET_DASICSLIBBOUNDS22(sp)
    csrw CSR_DLBOUND22, t0

    ld t0, OFFSET_DASICSLIBBOUNDS23(sp)
    csrw CSR_DLBOUND23, t0

    ld t0, OFFSET_DASICSLIBBOUNDS24(sp)
    csrw CSR_DLBOUND24, t0

    ld t0, OFFSET_DASICSLIBBOUNDS25(sp)
    csrw CSR_DLBOUND25, t0

    ld t0, OFFSET_DASICSLIBBOUNDS26(sp)
    csrw CSR_DLBOUND26, t0

    ld t0, OFFSET_DASICSLIBBOUNDS27(sp)
    csrw CSR_DLBOUND27, t0

    ld t0, OFFSET_DASICSLIBBOUNDS28(sp)
    csrw CSR_DLBOUND28, t0

    ld t0, OFFSET_DASICSLIBBOUNDS29(sp)
    csrw CSR_DLBOUND29, t0

    ld t0, OFFSET_DASICSLIBBOUNDS30(sp)
    csrw CSR_DLBOUND30, t0

    ld t0, OFFSET_DASICSLIBBOUNDS31(sp)
    csrw CSR_DLBOUND31, t0

    ld t0, OFFSET_DASICSMAINCALLENTRY(sp)
    csrw CSR_DMAINCALL, t0

    ld t0, OFFSET_DASICSRETURNPC(sp)
    csrw CSR_DRETURNPC, t0 

    ld t0, OFFSET_DASICSFREEZONERETURNPC(sp)   
    csrw CSR_DFZRETURN, t0

    addi sp, sp, OFFSET_DASICS   

//    ld t0, OFFSET_REG_USTATUS(sp)
//    csrw CSR_USTATUS, t0 

    ld t0, OFFSET_REG_UIE(sp)
    csrw CSR_UIE, t0 
    
    ld t0, OFFSET_REG_UTVEC(sp)
    csrw CSR_UTVEC, t0 

    ld t0, OFFSET_REG_USCRATCH(sp)
    csrw CSR_USCRATCH, t0
    
    ld t0, OFFSET_REG_UEPC(sp)
    csrw CSR_UEPC, t0 

    ld t0, OFFSET_REG_UCAUSE(sp)
    csrw CSR_UCAUSE, t0 

    ld t0, OFFSET_REG_UBADADDR(sp)
    csrw CSR_UTVAL, t0 

    ld t0, OFFSET_REG_UIP(sp)   
    csrw CSR_UIP, t0

    addi sp, sp, OFFSET_N_EXTENSION    

    // return address
    ld ra, OFFSET_REG_RA(sp)

    // pointers, sp will be handled later
    ld gp, OFFSET_REG_GP(sp)
    ld tp, OFFSET_REG_TP(sp)

    // temporary
    ld t0, OFFSET_REG_T0(sp)
    ld t1, OFFSET_REG_T1(sp)
    ld t2, OFFSET_REG_T2(sp)

    // saved register
    ld s0, OFFSET_REG_S0(sp)
    ld s1, OFFSET_REG_S1(sp)

    // args
    ld a0, OFFSET_REG_A0(sp)
    ld a1, OFFSET_REG_A1(sp)
    ld a2, OFFSET_REG_A2(sp)
    ld a3, OFFSET_REG_A3(sp)
    ld a4, OFFSET_REG_A4(sp)
    ld a5, OFFSET_REG_A5(sp)
    ld a6, OFFSET_REG_A6(sp)
    ld a7, OFFSET_REG_A7(sp)

    // saved register
    ld s2, OFFSET_REG_S2(sp)
    ld s3, OFFSET_REG_S3(sp)
    ld s4, OFFSET_REG_S4(sp)
    ld s5, OFFSET_REG_S5(sp)
    ld s6, OFFSET_REG_S6(sp)
    ld s7, OFFSET_REG_S7(sp)
    ld s8, OFFSET_REG_S8(sp)
    ld s9, OFFSET_REG_S9(sp)
    ld s10, OFFSET_REG_S10(sp)
    ld s11, OFFSET_REG_S11(sp)

    // temporary register
    ld t3, OFFSET_REG_T3(sp)
    ld t4, OFFSET_REG_T4(sp)
    ld t5, OFFSET_REG_T5(sp)
    ld t6, OFFSET_REG_T6(sp)

    addi sp, sp, OFFSET_SIZE
.endm


#--------------------------------------------------------------------
# __dasics_uret: ret from user exception.
#--------------------------------------------------------------------
ENTRY(__dasics_uret)
    RESTORE_CONTEXT
    uret
END(__dasics_uret)

#--------------------------------------------------------------------
# dasics_ufault_entry: entry function before handling dasics faults.
#--------------------------------------------------------------------
ENTRY(dasics_ufault_entry) 
    SAVE_CONTEXT
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop     
    mv a0, sp
    la t0, dasics_ufault_handler
    la ra, __dasics_uret
    jr t0
    ret
END(dasics_ufault_entry)

#-----------------------------------------------------------------------
# __dasics_start_ufault_entry: deal fault when the dynamic.
#-----------------------------------------------------------------------
ENTRY(__dasics_start_ufault_entry)
    SAVE_CONTEXT
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop  
    mv a0, sp
    la ra, __dasics_uret
    la t0, _dasics_start_ufault_entry
    jr t0
    ret
END(__dasics_start_ufault_entry)

#-----------------------------------------------------------------------
# dasics_umaincall: pass arguments to umaincall helper.
#-----------------------------------------------------------------------
ENTRY(dasics_umaincall)
    # addi sp, sp, -16
    # sd   ra, 8(sp)
    SAVE_CONTEXT
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop     
    mv   a0, sp
    ld   t0, umaincall_helper
    jalr t0
    RESTORE_CONTEXT
    # SAVE_CONTEXT
    # li a0, 1
    # la a1, msg 
    # li a2, 17
    # li a7, 64
    # ecall
    # call die_for
    # RESTORE_CONTEXT   
    # ld   ra, 8(sp)
    # addi sp, sp, 16
    # .word 0x0000f00b    # pulpret in little endian
    .word 0x0003700b      # pulpret to t1
END(dasics_umaincall)

.section .data 
.local msg   
msg: .string "let's over...\n\r"
