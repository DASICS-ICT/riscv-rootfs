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
     * Preserve and record the complete integer context.  Frame offsets 0..240
     * contain x1..x31 in architectural order, followed by the entry uscratch
     * and selected recovery PC.  The 272-byte frame keeps the ABI stack
     * alignment while trusted C helpers run in HU mode.
     */
    addi  sp, sp, -272
    sd    ra,   0(sp)      /* x1  */
    sd    t0,  32(sp)      /* x5: save before using it for original sp */
    addi  t0, sp, 272
    sd    t0,   8(sp)      /* x2  */
    sd    gp,  16(sp)      /* x3  */
    sd    tp,  24(sp)      /* x4  */
    sd    t1,  40(sp)      /* x6  */
    sd    t2,  48(sp)      /* x7  */
    sd    s0,  56(sp)      /* x8  */
    sd    s1,  64(sp)      /* x9  */
    sd    a0,  72(sp)      /* x10 */
    sd    a1,  80(sp)      /* x11 */
    sd    a2,  88(sp)      /* x12 */
    sd    a3,  96(sp)      /* x13 */
    sd    a4, 104(sp)      /* x14 */
    sd    a5, 112(sp)      /* x15 */
    sd    a6, 120(sp)      /* x16 */
    sd    a7, 128(sp)      /* x17 */
    sd    s2, 136(sp)      /* x18 */
    sd    s3, 144(sp)      /* x19 */
    sd    s4, 152(sp)      /* x20 */
    sd    s5, 160(sp)      /* x21 */
    sd    s6, 168(sp)      /* x22 */
    sd    s7, 176(sp)      /* x23 */
    sd    s8, 184(sp)      /* x24 */
    sd    s9, 192(sp)      /* x25 */
    sd    s10, 200(sp)     /* x26 */
    sd    s11, 208(sp)     /* x27 */
    sd    t3, 216(sp)      /* x28 */
    sd    t4, 224(sp)      /* x29 */
    sd    t5, 232(sp)      /* x30 */
    sd    t6, 240(sp)      /* x31 */

    csrr  t6, uscratch
    sd    t6, 248(sp)
    bnez  t6, .Ln_extension_recovery_ready
    csrr  t6, uepc
    addi  t6, t6, 4
.Ln_extension_recovery_ready:
    sd    t6, 256(sp)
    csrw  uscratch, zero

    /*
     * Keep one common ordered record stream for every N-profile workload.
     * The OFB and syscall tests retain their stronger case-specific records.
     */
    la    t1, dasics_n_extension_trap_count
    ld    t2, 0(t1)
    li    t3, 32
    bgeu  t2, t3, .Ln_extension_record_done
    la    t3, dasics_n_extension_trap_records
    li    t4, 336
    mul   t4, t2, t4
    add   t3, t3, t4
    csrr  t4, ustatus
    sd    t4, 0(t3)
    csrr  t4, uie
    sd    t4, 8(t3)
    csrr  t4, utvec
    sd    t4, 16(t3)
    ld    t4, 248(sp)
    sd    t4, 24(t3)
    csrr  t4, uepc
    sd    t4, 32(t3)
    csrr  t4, ucause
    sd    t4, 40(t3)
    csrr  t4, utval
    sd    t4, 48(t3)
    csrr  t4, uip
    sd    t4, 56(t3)
    csrr  t4, 0x8b3
    sd    t4, 64(t3)
    ld    t4, 256(sp)
    sd    t4, 72(t3)

    /* gpr[0] is x0; copy saved x1..x31 from the architectural frame. */
    sd    zero, 80(t3)
    li    t4, 0
    addi  t5, t3, 88
    li    t6, 31
.Ln_extension_copy_gpr:
    add   t0, sp, t4
    ld    t1, 0(t0)
    sd    t1, 0(t5)
    addi  t4, t4, 8
    addi  t5, t5, 8
    addi  t6, t6, -1
    bnez  t6, .Ln_extension_copy_gpr
.Ln_extension_record_done:
    addi  t2, t2, 1
    la    t1, dasics_n_extension_trap_count
    sd    t2, 0(t1)

    /* The full architectural frame is safe before the trace uses libc. */
    mv    a0, t2
    call  dasics_n_extension_user_trap_handler_trace

    csrr  t0, 0x8b3
    li    t1, 1
    bne   t0, t1, .Ln_extension_set_recovery
    ld    a0, 128(sp)
    ld    a1, 72(sp)
    ld    a2, 80(sp)
    ld    a3, 88(sp)
    ld    a4, 96(sp)
    ld    a5, 104(sp)
    ld    a6, 112(sp)
    call  dasics_n_extension_syscall_handler
    sd    a0, 72(sp)

.Ln_extension_set_recovery:
    ld    t0, 256(sp)
    csrw  uepc, t0

    ld    ra,   0(sp)
    ld    gp,  16(sp)
    ld    tp,  24(sp)
    ld    t0,  32(sp)
    ld    t1,  40(sp)
    ld    t2,  48(sp)
    ld    s0,  56(sp)
    ld    s1,  64(sp)
    ld    a0,  72(sp)
    ld    a1,  80(sp)
    ld    a2,  88(sp)
    ld    a3,  96(sp)
    ld    a4, 104(sp)
    ld    a5, 112(sp)
    ld    a6, 120(sp)
    ld    a7, 128(sp)
    ld    s2, 136(sp)
    ld    s3, 144(sp)
    ld    s4, 152(sp)
    ld    s5, 160(sp)
    ld    s6, 168(sp)
    ld    s7, 176(sp)
    ld    s8, 184(sp)
    ld    s9, 192(sp)
    ld    s10, 200(sp)
    ld    s11, 208(sp)
    ld    t3, 216(sp)
    ld    t4, 224(sp)
    ld    t5, 232(sp)
    ld    t6, 240(sp)
    addi  sp, sp, 272
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
.size dasics_n_extension_trap_records, 10752
dasics_n_extension_trap_records:
    .zero 10752
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
