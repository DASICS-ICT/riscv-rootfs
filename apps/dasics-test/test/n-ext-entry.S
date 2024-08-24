.section .text
.global dasics_ufault_entry
.type dasics_ufault_entry, @function
.align 2, 0x13  # nop = addi x0, x0, 0

dasics_ufault_entry: 
    addi sp, sp, -16
    sd   ra, 8(sp)
    call u_int_handler
    ld   ra, 8(sp)
    addi sp, sp, 16
    uret 
