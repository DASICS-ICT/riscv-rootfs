#--------------------------------------------------------------------
# dasics_ufault_entry: entry function before handling dasics faults.
#--------------------------------------------------------------------

.section .text
.global dasics_ufault_entry
.type dasics_ufault_entry, @function
.align 2, 0x13  # nop = addi x0, x0, 0

dasics_ufault_entry: 
    addi sp, sp, -72
    sd   ra, 64(sp)
    sd   a0, 56(sp)
    sd   a1, 48(sp)
    sd   a2, 40(sp)
    sd   a3, 32(sp)
    sd   a4, 24(sp)
    sd   a5, 16(sp)
    sd   a6, 8(sp)
    sd   a7, 0(sp)
    call dasics_ufault_handler
    ld   ra, 64(sp)
    ld   a0, 56(sp)
    ld   a1, 48(sp)
    ld   a2, 40(sp)
    ld   a3, 32(sp)
    ld   a4, 24(sp)
    ld   a5, 16(sp)
    ld   a6, 8(sp)
    ld   a7, 0(sp)
    addi sp, sp, 72
    uret 

#-----------------------------------------------------------------------
# dasics_umaincall: pass arguments to umaincall helper.
#-----------------------------------------------------------------------
.section .text
.global dasics_umaincall
.type dasics_umaincall, @function
.extern uint64_t umaincall_helper 

dasics_umaincall:
    addi sp, sp, -16
    sd   ra, 8(sp)
    ld   t0, umaincall_helper
    jalr t0
    ld   ra, 8(sp)
    addi sp, sp, 16
    ret
    //.word 0x0000f00b    # pulpret in little endian