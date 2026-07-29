#

#--------------------------------------------------------------------
# dasics_ufault_entry: entry function before handling dasics faults.
#--------------------------------------------------------------------

.section .text
#ifndef DASICS_S_TRAP_ONLY
.global dasics_ufault_entry
.type dasics_ufault_entry, @function
.align 2, 0x13  # nop = addi x0, x0, 0

dasics_ufault_entry: 
	#ifdef DASICS_N_EXTENSION_PROFILE
    /*
     * The N profile deliberately uses a minimal, non-production handler.
     * uscratch carries the explicit recovery PC.  t0 is restored before
     * uret; t1-t4 are caller-saved and are declared clobbered by the two
     * profile-only fault sites.
     */
    addi  sp, sp, -128
    sd    ra,   0(sp)
    sd    t0,   8(sp)
    sd    t1,  16(sp)
    sd    t2,  24(sp)
    sd    t3,  32(sp)
    sd    t4,  40(sp)
    sd    t5,  48(sp)
    sd    t6,  56(sp)
    sd    a0,  64(sp)
    sd    a1,  72(sp)
    sd    a2,  80(sp)
    sd    a3,  88(sp)
    sd    a4,  96(sp)
    sd    a5, 104(sp)
    sd    a6, 112(sp)
    sd    a7, 120(sp)

    csrr  t0, 0x8b3
    li    t1, 1
    beq   t0, t1, .Ln_extension_syscall

    ld    ra,   0(sp)
    ld    t0,   8(sp)
    ld    t1,  16(sp)
    ld    t2,  24(sp)
    ld    t3,  32(sp)
    ld    t4,  40(sp)
    ld    t5,  48(sp)
    ld    t6,  56(sp)
    ld    a0,  64(sp)
    ld    a1,  72(sp)
    ld    a2,  80(sp)
    ld    a3,  88(sp)
    ld    a4,  96(sp)
    ld    a5, 104(sp)
    ld    a6, 112(sp)
    ld    a7, 120(sp)
    addi  sp, sp, 128

    csrrw t0, uscratch, t0
    la    t1, dasics_n_extension_trap_count
    ld    t2, 0(t1)
    li    t3, 2
    bgeu  t2, t3, 2f
    la    t3, dasics_n_extension_trap_records
    beqz  t2, 1f
    addi  t3, t3, 48
1:
    csrr  t4, ustatus
    sd    t4, 0(t3)
    csrr  t4, uepc
    sd    t4, 8(t3)
    csrr  t4, ucause
    sd    t4, 16(t3)
    csrr  t4, utval
    sd    t4, 24(t3)
    csrr  t4, 0x8b3
    sd    t4, 32(t3)
    sd    t0, 40(t3)
    addi  t2, t2, 1
    sd    t2, 0(t1)
2:
    csrw  uepc, t0
    csrrw t0, uscratch, t0
    uret

.Ln_extension_syscall:
    mv    a0, a7
    ld    a1, 64(sp)
    ld    a2, 72(sp)
    ld    a3, 80(sp)
    ld    a4, 88(sp)
    ld    a5, 96(sp)
    ld    a6, 104(sp)
    call  dasics_n_extension_syscall_handler
    sd    a0, 64(sp)

    csrr  t0, uepc
    addi  t0, t0, 4
    csrw  uepc, t0

    ld    ra,   0(sp)
    ld    t0,   8(sp)
    ld    t1,  16(sp)
    ld    t2,  24(sp)
    ld    t3,  32(sp)
    ld    t4,  40(sp)
    ld    t5,  48(sp)
    ld    t6,  56(sp)
    ld    a0,  64(sp)
    ld    a1,  72(sp)
    ld    a2,  80(sp)
    ld    a3,  88(sp)
    ld    a4,  96(sp)
    ld    a5, 104(sp)
    ld    a6, 112(sp)
    ld    a7, 120(sp)
    addi  sp, sp, 128
    uret
	#else
    addi sp, sp, -16
    sd   ra, 8(sp)
    call dasics_ufault_handler
    ld   ra, 8(sp)
    addi sp, sp, 16
    uret 
#endif
#endif

#ifdef DASICS_N_EXTENSION_PROFILE
.section .bss
.align 3
.global dasics_n_extension_trap_count
.type dasics_n_extension_trap_count, @object
.size dasics_n_extension_trap_count, 8
dasics_n_extension_trap_count:
    .zero 8

.global dasics_n_extension_trap_records
.type dasics_n_extension_trap_records, @object
.size dasics_n_extension_trap_records, 96
dasics_n_extension_trap_records:
    .zero 96
#endif

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
    li   t1, 1
    bne  a0, t1, 1f
    addi t1, ra, 4
    csrw 0x8b2, t1
1:
    ld   t0, umaincall_helper
    jalr t0
    ld   ra, 8(sp)
    addi sp, sp, 16
    ret
    //.word 0x0000f00b    # pulpret in little endian
