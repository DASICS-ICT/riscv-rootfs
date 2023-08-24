#-----------------------------------------------------------------------
# : pass arguments to umaincall helper.
#-----------------------------------------------------------------------
.section .text
.global lib_call
.type lib_call, @function

lib_call:
    addi   sp, sp, -8
    sd     ra, 0(sp)
    .word 0x0005108b    // dasicscall.jr ra, a0
    ld     ra, 0(sp)
    addi   sp, sp, 8
    ret

#-----------------------------------------------------------------------
# : pass arguments to umaincall helper.
#-----------------------------------------------------------------------
.section .text.ufreezonetext
.global azone_call
.type azone_call, @function

azone_call:
    addi   sp, sp, -8
    sd     ra, 0(sp)
    auipc  t0, 0
    addi   t0, t0, 16
    csrw   0x8b2, t0
    jalr   ra, a0
    ld     ra, 0(sp)
    addi   sp, sp, 8
    ret
