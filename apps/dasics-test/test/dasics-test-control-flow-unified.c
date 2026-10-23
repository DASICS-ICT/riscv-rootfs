#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "udasics.h"

#define CONTROL_TAG "[DASICS-CONTROL-FLOW]"

#if defined(DASICS_N_EXTENSION_PROFILE) && DASICS_LINUX_DUAL_EXEC
#define CONTROL_RA_SEED 0x1122334455667788UL
#define CONTROL_T2_SEED 0x2233445566778899UL
#define CONTROL_ULIB_MARKER 0x554c494253454354UL
#define CONTROL_MARKER_POISON 0x4a554d50504f4953UL
#define CONTROL_MARKER_REACHED 0x4a554d5054414b45UL

/* Only the trusted wrappers write this result. Probe destinations are real
 * architectural registers, captured before the untrusted return restores x1. */
struct control_observation {
    unsigned long hits, ra, target_pc, return_pc, freason, uscratch, t2, marker;
    unsigned long maincall, active, jump_cfg;
};

struct control_library_cells {
    unsigned long marker, rd, target_pc;
};

static volatile struct control_library_cells control_library_cells
    __attribute__((section(".ulibdata"), aligned(8), used));

extern void control_invoke(struct control_observation *, char *, char *, char *);
extern void control_invoke_special(struct control_observation *, char *, char *,
                                   char *, unsigned long, char *);
extern int control_library_entry(void);
extern char control_bound_return[], control_special_return[];
extern char control_jal_entry[], control_jal_site[], control_jal_recovery[];
extern char control_beq_entry[], control_beq_site[], control_beq_recovery[];
extern char control_jal_trusted_entry[], control_jal_trusted_site[], control_jal_trusted_recovery[];
extern char control_not_taken_entry[], control_not_taken_site[], control_not_taken_recovery[];
extern char control_jalr_entry[], control_jalr_site[], control_jalr_recovery[];
extern char control_jump_target[], control_branch_target[], control_trusted_target[];
extern char control_special_target[], control_special_target_previous[], control_special_target_next[];
extern char control_section_ulib_entry[];
extern char control_library_site[], control_library_recovery[], control_library_main_target[];

/* FDICALL_JR x1,x5,0 uses custom3. The ordinary-entry wrapper is separate:
 * FDICALL would overwrite the ReturnPC under test in the special scenarios. */
#define CONTROL_WRAPPER_START(name) \
    ".section .text,\"ax\",@progbits\n.balign 8\n.global " #name "\n" \
    ".type " #name ",@function\n" #name ":\n" \
    "addi sp,sp,-16\nsd ra,8(sp)\nsd a0,0(sp)\n" \
    "mv t0,a1\nmv t3,a2\nmv t5,a3\n"
#define CONTROL_WRAPPER_FINISH(name, ret) \
    ".global " #ret "\n" #ret ":\n" \
    "ld t0,0(sp)\nsd a1,0(t0)\nsd a4,8(t0)\nsd t4,16(t0)\n" \
    "csrr t1,0x8b1\nsd t1,24(t0)\ncsrr t1,0x8b3\nsd t1,32(t0)\n" \
    "csrr t1,uscratch\nsd t1,40(t0)\nsd t2,48(t0)\nsd a0,56(t0)\n" \
    "csrr t1,0x8b0\nsd t1,64(t0)\ncsrr t1,0x8b2\nsd t1,72(t0)\n" \
    "csrr t1,0x8c8\nsd t1,80(t0)\ncsrw uscratch,zero\n" \
    "ld ra,8(sp)\naddi sp,sp,16\nret\n.size " #name ",.-" #name "\n"
#define CONTROL_PROBE_START(name) \
    ".section .ulibtext,\"ax\",@progbits\n.balign 8\n.global " #name "_entry\n" \
    ".type " #name "_entry,@function\n" #name "_entry:\n" \
    "mv t6,ra\nli ra,0x1122334455667788\nli t2,0x2233445566778899\n" \
    "li a1,0\nli t4,0\nli t0,1\n.global " #name "_site\n" #name "_site:\n"
#define CONTROL_PROBE_FINISH(name) \
    ".global " #name "_recovery\n" #name "_recovery:\n" \
    "mv a4,ra\nmv ra,t6\nret\n.size " #name "_entry,.-" #name "_entry\n"

asm(
    ".option push\n.option norvc\n.option norelax\n"
    CONTROL_WRAPPER_START(control_invoke)
    "csrw 0x8b2,t5\nli a1,0\nli a4,0\nli t4,0\n"
    ".global control_bound_call_site\ncontrol_bound_call_site:\n"
    ".word 0x000290fb\n"
    CONTROL_WRAPPER_FINISH(control_invoke, control_bound_return)

    CONTROL_WRAPPER_START(control_invoke_special)
    "la t4,control_special_return\ncsrw 0x8b0,zero\n"
    "csrw 0x8b1,t4\ncsrw 0x8b2,t5\n"
    "beqz a4,1f\nli t1,1\nbeq a4,t1,2f\n"
    "csrw 0x8b2,a5\nj 3f\n"
    "1:\ncsrw 0x8b0,a5\nj 3f\n"
    "2:\ncsrw 0x8b1,a5\ncsrw 0x8b0,t4\n"
    "3:\nli a1,0\nli a4,0\nli t4,0\n"
    ".global control_special_entry_site\ncontrol_special_entry_site:\njalr ra,0(t0)\n"
    CONTROL_WRAPPER_FINISH(control_invoke_special, control_special_return)

    CONTROL_PROBE_START(control_jal)
    "jal ra,control_jump_target\n"
    CONTROL_PROBE_FINISH(control_jal)
    CONTROL_PROBE_START(control_beq)
    "beq t0,t0,control_branch_target\n"
    CONTROL_PROBE_FINISH(control_beq)
    CONTROL_PROBE_START(control_jal_trusted)
    "jal ra,control_trusted_target\n"
    CONTROL_PROBE_FINISH(control_jal_trusted)
    CONTROL_PROBE_START(control_not_taken)
    "beq t0,zero,control_branch_target\n"
    CONTROL_PROBE_FINISH(control_not_taken)
    CONTROL_PROBE_START(control_jalr)
    "jalr t2,0(t3)\n"
    CONTROL_PROBE_FINISH(control_jalr)

    ".section .ulibtext,\"ax\",@progbits\n.balign 8\n"
    ".global control_branch_target\ncontrol_branch_target:\n"
    "auipc t4,0\naddi a1,a1,1\njalr zero,0(t5)\n"
    ".balign 8\n.global control_section_ulib_entry\ncontrol_section_ulib_entry:\n"
    "li a0,0x554c494253454354\nret\n"

    ".section .ufreezonetext,\"ax\",@progbits\n.balign 8\n"
    ".global control_jump_target\ncontrol_jump_target:\n"
    "auipc t4,0\naddi a1,a1,1\njalr zero,0(t5)\n"

    ".section .text,\"ax\",@progbits\n.balign 8\n"
    ".global control_trusted_target\ncontrol_trusted_target:\n"
    "auipc t4,0\naddi a1,a1,1\njalr zero,0(t5)\n"
    /* Both forbidden neighbours remain safe if protection is broken. The
     * +4 neighbour shares the exact target's 8-byte granule. */
    ".balign 8\nnop\n.global control_special_target_previous\n"
    "control_special_target_previous:\nauipc t4,0\n"
    ".global control_special_target\ncontrol_special_target:\nauipc t4,0\n"
    ".global control_special_target_next\ncontrol_special_target_next:\n"
    "addi a1,a1,1\njalr zero,0(t5)\n"

    ".section .ulibtext,\"ax\",@progbits\n.balign 8\n"
    ".global control_library_entry\n.type control_library_entry,@function\n"
    "control_library_entry:\nmv t6,ra\nli ra,0x1122334455667788\n"
    "li t2,0x2233445566778899\nla t3,control_library_main_target\n"
    ".global control_library_site\ncontrol_library_site:\n"
    "jal t2,control_library_main_target\n"
    ".global control_library_recovery\ncontrol_library_recovery:\n"
    "la t0,control_library_cells\nsd t2,8(t0)\nmv ra,t6\nli a0,0\nret\n"
    ".size control_library_entry,.-control_library_entry\n"
    ".section .text,\"ax\",@progbits\n.balign 8\n"
    ".global control_library_main_target\ncontrol_library_main_target:\n"
    "auipc t3,0\nla t0,control_library_cells\nli t1,0x4a554d5054414b45\n"
    "sd t1,0(t0)\nsd t3,16(t0)\njalr zero,0(t2)\n"
    ".option pop\n"
);

/* Library allocation changes one selected memory slot. Snapshot the bank so
 * cleanup also restores its old, inactive bound payload after free clears V. */
struct control_state {
    unsigned long maincall, return_pc, active, freason, jump_cfg, uscratch;
    unsigned long jump[8], lib_cfg, lib[32];
};

#define CONTROL_MEMORY_SLOTS(M) \
    M(0) M(1) M(2) M(3) M(4) M(5) M(6) M(7) \
    M(8) M(9) M(10) M(11) M(12) M(13) M(14) M(15)
#define CONTROL_MEMORY_READ(index) \
    state.lib[2 * index] = csr_read(0x890 + 2 * index); \
    state.lib[2 * index + 1] = csr_read(0x891 + 2 * index);
#define CONTROL_MEMORY_WRITE(index) \
    csr_write(0x890 + 2 * index, state->lib[2 * index]); \
    csr_write(0x891 + 2 * index, state->lib[2 * index + 1]);

static struct control_state control_read_state(void)
{
    struct control_state state = {
        .maincall = csr_read(0x8b0), .return_pc = csr_read(0x8b1),
        .active = csr_read(0x8b2), .freason = csr_read(0x8b3),
        .jump_cfg = csr_read(0x8c8), .uscratch = csr_read(CSR_USCRATCH),
        .jump = {csr_read(0x8c0), csr_read(0x8c1), csr_read(0x8c2), csr_read(0x8c3),
                 csr_read(0x8c4), csr_read(0x8c5), csr_read(0x8c6), csr_read(0x8c7)},
        .lib_cfg = csr_read(0x880)
    };
    CONTROL_MEMORY_SLOTS(CONTROL_MEMORY_READ)
    return state;
}

static int control_restore_state(const struct control_state *state)
{
    csr_write(0x8c8, 0);
    csr_write(0x880, 0);
    CONTROL_MEMORY_SLOTS(CONTROL_MEMORY_WRITE)
    csr_write(0x8c0, state->jump[0]); csr_write(0x8c1, state->jump[1]);
    csr_write(0x8c2, state->jump[2]); csr_write(0x8c3, state->jump[3]);
    csr_write(0x8c4, state->jump[4]); csr_write(0x8c5, state->jump[5]);
    csr_write(0x8c6, state->jump[6]); csr_write(0x8c7, state->jump[7]);
    csr_write(0x8b0, state->maincall); csr_write(0x8b1, state->return_pc);
    csr_write(0x8b2, state->active); csr_write(0x8b3, state->freason);
    csr_write(CSR_USCRATCH, state->uscratch);
    csr_write(0x8c8, state->jump_cfg);
    csr_write(0x880, state->lib_cfg);
    struct control_state actual = control_read_state();
    return memcmp(&actual, state, sizeof(actual)) == 0;
}

static void control_clear_jump(void)
{
    csr_write(0x8c8, 0);
    csr_write(0x8c0, 0); csr_write(0x8c1, 0);
    csr_write(0x8c2, 0); csr_write(0x8c3, 0);
    csr_write(0x8c4, 0); csr_write(0x8c5, 0);
    csr_write(0x8c6, 0); csr_write(0x8c7, 0);
    csr_write(0x8b0, 0); csr_write(0x8b1, 0); csr_write(0x8b2, 0);
}

static void control_set_bound(unsigned int slot, unsigned long lo, unsigned long hi,
                              unsigned long cfg)
{
    control_clear_jump();
    switch (slot) {
    case 0: csr_write(0x8c0, lo); csr_write(0x8c1, hi); break;
    case 1: csr_write(0x8c2, lo); csr_write(0x8c3, hi); break;
    case 2: csr_write(0x8c4, lo); csr_write(0x8c5, hi); break;
    case 3: csr_write(0x8c6, lo); csr_write(0x8c7, hi); break;
    }
    csr_write(0x8c8, cfg);
}

static int control_value(const char *name, unsigned long actual, unsigned long expected,
                          int context_ok, unsigned long *total)
{
    int pass = context_ok && actual == expected;
    (*total)++;
    printf(CONTROL_TAG " case=%s value=0x%lx expect=0x%lx context_ok=%d result=%s\n",
           name, actual, expected, context_ok, pass ? "PASS" : "FAIL");
    return !pass;
}

struct control_case {
    const char *name;
    char *entry, *site, *recovery, *target;
    unsigned int slot;
    unsigned long lo, hi, cfg;
    int branch, taken, allow, special;
};

static int control_fault_context(unsigned long before, unsigned long after,
                                  char *site, char *target, char *recovery,
                                  unsigned long outer_return)
{
    if (before >= DASICS_N_EXTENSION_TRAP_RECORDS || after != before + 1)
        return 0;
    const volatile dasics_n_extension_trap_record_t *record =
        &dasics_n_extension_trap_records[before];
    return record->ucause == DASICS_N_EXTENSION_UCHECK_CAUSE &&
        record->uepc == (unsigned long)site && record->utval == (unsigned long)target &&
        record->dfreason == EXC_DASICS_JUMP_FAULT &&
        record->uscratch == (unsigned long)recovery &&
        record->recovery == (unsigned long)recovery &&
        record->utvec == (unsigned long)dasics_ufault_entry &&
        (record->ustatus & 0x11UL) == 0x10UL &&
        record->gpr[0] == 0 && record->gpr[1] == CONTROL_RA_SEED &&
        record->gpr[7] == CONTROL_T2_SEED && record->gpr[11] == 0 &&
        record->gpr[28] == (unsigned long)target &&
        record->gpr[29] == 0 && record->gpr[30] == (unsigned long)recovery &&
        record->gpr[31] == outer_return;
}

static int control_run_case(const struct control_case *test, unsigned long *total)
{
    struct control_observation observed = {0};
    int denied = test->taken && !test->allow;
    int indirect = test->entry == control_jalr_entry;
    unsigned long expected_outer = (unsigned long)(test->special < 0 ?
        control_bound_return : control_special_return);
    unsigned long expected_return = test->special == 1 ?
        (unsigned long)control_special_target : expected_outer;
    unsigned long expected_maincall = test->special == 0 ?
        (unsigned long)control_special_target : test->special == 1 ? expected_outer : 0;
    unsigned long expected_active = test->special == 2 ?
        (unsigned long)control_special_target : (unsigned long)test->recovery;
    unsigned long expected_hits = test->taken && test->allow;
    unsigned long rd_seed = indirect ? CONTROL_T2_SEED : CONTROL_RA_SEED;
    unsigned long expected_rd = expected_hits && !test->branch ?
        (unsigned long)test->site + 4 : rd_seed;
    unsigned long before = dasics_n_extension_trap_count;

    control_set_bound(test->slot, test->lo, test->hi, test->cfg);
    struct control_state configured = control_read_state();
    int setup_ok = configured.jump_cfg == test->cfg &&
        configured.jump[2 * test->slot] == test->lo &&
        configured.jump[2 * test->slot + 1] == test->hi;
    if (test->special >= 0) {
        /* A neighbour must not equal either independent safe-return grant.
         * All jump V bits are zero for these exact-PC comparisons. */
        setup_ok = setup_ok && !configured.jump_cfg &&
            (test->allow || ((unsigned long)test->target != expected_return &&
                            (unsigned long)test->target != expected_maincall &&
                            (unsigned long)test->target != expected_active));
    }
    csr_write(0x8b3, 0);
    csr_write(CSR_USCRATCH, (unsigned long)test->recovery);
    if (test->special < 0)
        control_invoke(&observed, test->entry, test->target, test->recovery);
    else
        control_invoke_special(&observed, test->entry, test->target, test->recovery,
                               (unsigned long)test->special, control_special_target);
    unsigned long after = dasics_n_extension_trap_count;
    unsigned long rd = indirect ? observed.t2 : observed.ra;
    unsigned long expected_t2 = indirect ? expected_rd : CONTROL_T2_SEED;
    unsigned long expected_ra = indirect ? CONTROL_RA_SEED : expected_rd;
    int context_ok = !denied || control_fault_context(before, after, test->site,
        test->target, test->recovery, expected_outer);
    int pass = setup_ok && context_ok && observed.hits == expected_hits &&
        observed.target_pc == (expected_hits ? (unsigned long)test->target : 0) &&
        rd == expected_rd && observed.ra == expected_ra && observed.t2 == expected_t2 &&
        observed.return_pc == expected_return && observed.maincall == expected_maincall &&
        observed.active == expected_active && observed.jump_cfg == test->cfg &&
        observed.freason == (denied ? EXC_DASICS_JUMP_FAULT : 0) &&
        observed.uscratch == (denied ? 0 : (unsigned long)test->recovery) &&
        csr_read(CSR_USCRATCH) == 0 && after == before + denied;
    unsigned long cause = 0, epc = 0, tval = 0, recovery = 0;
    if (denied && before < DASICS_N_EXTENSION_TRAP_RECORDS && after > before) {
        const volatile dasics_n_extension_trap_record_t *record =
            &dasics_n_extension_trap_records[before];
        cause = record->ucause; epc = record->uepc; tval = record->utval;
        recovery = record->recovery;
    }
    (*total)++;
    printf(CONTROL_TAG " case=%s value=0x%lx expect=0x1 source=0x%lx site=0x%lx "
           "target=0x%lx target_pc=0x%lx target_pc_expected=0x%lx "
           "recovery=0x%lx rd=0x%lx rd_expected=0x%lx ra=0x%lx ra_expected=0x%lx "
           "t2=0x%lx t2_expected=0x%lx hits=%lu hits_expected=%lu "
           "slot=%u cfg=0x%lx cfg_expected=0x%lx lo=0x%lx lo_expected=0x%lx "
           "hi=0x%lx hi_expected=0x%lx maincall=0x%lx maincall_expected=0x%lx "
           "returnpc=0x%lx returnpc_expected=0x%lx active=0x%lx active_expected=0x%lx "
           "freason=0x%lx freason_expected=0x%lx uscratch=0x%lx uscratch_expected=0x%lx "
           "u24_before=%lu u24_after=%lu u24_expected=%lu "
           "cause=0x%lx epc=0x%lx tval=0x%lx recovered=0x%lx result=%s\n",
           test->name, (unsigned long)pass, (unsigned long)test->entry,
           (unsigned long)test->site, (unsigned long)test->target,
           observed.target_pc, expected_hits ? (unsigned long)test->target : 0,
           (unsigned long)test->recovery, rd, expected_rd, observed.ra, expected_ra,
           observed.t2, expected_t2, observed.hits, expected_hits, test->slot,
           configured.jump_cfg, test->cfg, configured.jump[2 * test->slot], test->lo,
           configured.jump[2 * test->slot + 1], test->hi,
           observed.maincall, expected_maincall, observed.return_pc, expected_return,
           observed.active, expected_active, observed.freason,
           denied ? EXC_DASICS_JUMP_FAULT : 0UL, observed.uscratch,
           denied ? 0UL : (unsigned long)test->recovery, before, after, before + denied,
           cause, epc, tval, recovery, pass ? "PASS" : "FAIL");
    return !pass;
}

static struct control_case control_direct_case(const char *name, int branch)
{
    struct control_case test = {
        .name = name,
        .entry = branch ? control_beq_entry : control_jal_entry,
        .site = branch ? control_beq_site : control_jal_site,
        .recovery = branch ? control_beq_recovery : control_jal_recovery,
        .target = branch ? control_branch_target : control_jump_target,
        .branch = branch, .taken = 1, .special = -1
    };
    return test;
}

static int control_direct(unsigned long *total)
{
    int failures = 0;
    char name[80];
    for (unsigned int slot = 0; slot < 4; slot++)
        for (int branch = 0; branch < 2; branch++)
            for (int allow = 1; allow >= 0; allow--) {
                snprintf(name, sizeof(name), "SLOT%u-%s-%s", slot, branch ? "BEQ" : "JAL",
                         allow ? "V-ALLOW" : "NONV-DENY");
                struct control_case test = control_direct_case(name, branch);
                test.slot = slot;
                test.allow = allow;
                test.lo = (unsigned long)test.target & ~7UL;
                test.hi = test.lo + 8;
                test.cfg = (allow ? 1UL : 0xfffeUL) << (slot * 16);
                failures += control_run_case(&test, total);
            }
    struct control_case trusted = control_direct_case("JAL-TRUSTED-NOBOUND-DENY", 0);
    trusted.entry = control_jal_trusted_entry;
    trusted.site = control_jal_trusted_site;
    trusted.recovery = control_jal_trusted_recovery;
    trusted.target = control_trusted_target;
    failures += control_run_case(&trusted, total);
    struct control_case nobound = control_direct_case("BEQ-NOBOUND-DENY", 1);
    failures += control_run_case(&nobound, total);

    static const char *boundaries[] = {"LOWER", "LAST-GRANULE", "UPPER", "BELOW", "EMPTY", "REVERSE"};
    for (int branch = 0; branch < 2; branch++)
        for (unsigned int edge = 0; edge < 6; edge++) {
            snprintf(name, sizeof(name), "%s-BOUND-%s", branch ? "BEQ" : "JAL", boundaries[edge]);
            struct control_case test = control_direct_case(name, branch);
            unsigned long target = (unsigned long)test.target;
            test.cfg = 1;
            test.allow = edge < 2;
            test.lo = edge == 0 || edge == 4 ? target :
                edge == 1 || edge == 2 ? target - 8 : target + 8;
            test.hi = edge < 2 ? target + 8 : edge == 3 ? target + 16 : target;
            failures += control_run_case(&test, total);
        }
    struct control_case not_taken = control_direct_case("BEQ-NOT-TAKEN-NOBOUND", 1);
    not_taken.entry = control_not_taken_entry;
    not_taken.site = control_not_taken_site;
    not_taken.recovery = control_not_taken_recovery;
    not_taken.taken = 0;
    not_taken.allow = 1;
    failures += control_run_case(&not_taken, total);
    return failures;
}

static int control_special(unsigned long *total)
{
    static const char *names[] = {"MAINCALL", "RETURNPC", "ACTIVE"};
    static const char *offsets[] = {"MINUS4", "EXACT", "PLUS4"};
    char name[80];
    int failures = 0;
    for (unsigned int special = 0; special < 3; special++)
        for (unsigned int offset = 0; offset < 3; offset++) {
            snprintf(name, sizeof(name), "SPECIAL-%s-%s", names[special], offsets[offset]);
            struct control_case test = {
                .name = name, .entry = control_jalr_entry, .site = control_jalr_site,
                .recovery = control_jalr_recovery,
                .target = offset == 0 ? control_special_target_previous :
                          offset == 1 ? control_special_target : control_special_target_next,
                .taken = 1, .allow = offset == 1, .special = (int)special
            };
            failures += control_run_case(&test, total);
        }
    return failures;
}

static int control_library(int enabled, unsigned long *total, int *cleanup_ok)
{
    struct control_state before_state = control_read_state();
    unsigned long lo = DASICS_BOUND_ALIGN_DOWN(&control_library_cells);
    unsigned long hi = DASICS_BOUND_ALIGN_UP(&control_library_cells + 1);
    int failures = 0, expected_slot = -1;
    for (unsigned int slot = 0; slot < 16; slot++)
        if (!(before_state.lib_cfg & (DASICS_LIBCFG_V << (slot * 4)))) {
            expected_slot = (int)slot;
            break;
        }
    int slot = dasics_libcfg_alloc(DASICS_LIBCFG_R | DASICS_LIBCFG_W, lo, hi);
    struct control_state allocated = control_read_state();
    int setup_ok = slot >= 0 && slot == expected_slot && slot < 16 &&
        allocated.lib[2 * slot] == lo && allocated.lib[2 * slot + 1] == hi &&
        allocated.lib_cfg == ((before_state.lib_cfg & ~(0xfUL << (slot * 4))) |
                             (0xbUL << (slot * 4)));
    failures += control_value("LIBRARY-SETUP-MARKER-BOUND", setup_ok, 1, 1, total);
    printf("CONTROL_LIBRARY_BOUND slot=%d slot_expected=%d lo=0x%lx lo_expected=0x%lx "
           "hi=0x%lx hi_expected=0x%lx "
           "cfg=0x%lx result=%s\n", slot, expected_slot,
           slot >= 0 && slot < 16 ? allocated.lib[2 * slot] : 0UL, lo,
           slot >= 0 && slot < 16 ? allocated.lib[2 * slot + 1] : 0UL, hi,
           allocated.lib_cfg, setup_ok ? "PASS" : "FAIL");
    control_library_cells.marker = CONTROL_MARKER_POISON;
    control_library_cells.rd = CONTROL_T2_SEED;
    control_library_cells.target_pc = 0;
    unsigned long before = dasics_n_extension_trap_count;
    csr_write(0x8b3, 0);
    csr_write(CSR_USCRATCH, (unsigned long)control_library_recovery);
    if (setup_ok) {
        if (enabled)
            lib_call(control_library_entry);
        else
            control_library_entry();
    }
    unsigned long after = dasics_n_extension_trap_count;
    unsigned long reason = csr_read(0x8b3);
    unsigned long pending = csr_read(CSR_USCRATCH);
    csr_write(CSR_USCRATCH, 0);
    unsigned long rd_expected = enabled ? CONTROL_T2_SEED : (unsigned long)control_library_site + 4;
    int context_ok = setup_ok && after == before + enabled &&
        control_library_cells.rd == rd_expected &&
        reason == (enabled ? 4UL : 0UL) &&
        pending == (enabled ? 0 : (unsigned long)control_library_recovery);
    if (enabled && before < DASICS_N_EXTENSION_TRAP_RECORDS && after == before + 1) {
        const volatile dasics_n_extension_trap_record_t *record =
            &dasics_n_extension_trap_records[before];
        /* The library return label belongs to lib_call. Its actual ReturnPC
         * is checked against the saved x31 rather than guessed from RVC size. */
        context_ok = context_ok && record->ucause == 24 &&
            record->uepc == (unsigned long)control_library_site &&
            record->utval == (unsigned long)control_library_main_target &&
            record->dfreason == 4 && record->uscratch == (unsigned long)control_library_recovery &&
            record->recovery == (unsigned long)control_library_recovery &&
            record->utvec == (unsigned long)dasics_ufault_entry &&
            (record->ustatus & 0x11UL) == 0x10UL &&
            record->gpr[0] == 0 && record->gpr[1] == CONTROL_RA_SEED &&
            record->gpr[7] == CONTROL_T2_SEED &&
            record->gpr[28] == (unsigned long)control_library_main_target &&
            record->gpr[31] == csr_read(0x8b1);
    } else if (enabled) {
        context_ok = 0;
    }
    if (enabled)
        failures += control_value("LIBRARY-ON-JUMP-REASON", reason, 4, context_ok, total);
    failures += control_value(enabled ? "LIBRARY-ON-MAIN-NOT-REACHED" : "LIBRARY-OFF-MAIN-REACHED",
        control_library_cells.marker, enabled ? CONTROL_MARKER_POISON : CONTROL_MARKER_REACHED,
        context_ok && control_library_cells.target_pc ==
            (enabled ? 0 : (unsigned long)control_library_main_target), total);
    printf("CONTROL_LIBRARY_OBSERVATION version=1 enabled=%d source=0x%lx site=0x%lx "
           "target=0x%lx recovery=0x%lx rd=0x%lx rd_expected=0x%lx "
           "target_pc=0x%lx target_pc_expected=0x%lx freason=0x%lx freason_expected=0x%lx "
           "u24_before=%lu u24_after=%lu u24_expected=%lu context_ok=%d result=%s\n",
           enabled, (unsigned long)control_library_entry, (unsigned long)control_library_site,
           (unsigned long)control_library_main_target, (unsigned long)control_library_recovery,
           control_library_cells.rd, rd_expected, control_library_cells.target_pc,
           enabled ? 0UL : (unsigned long)control_library_main_target, reason, enabled ? 4UL : 0UL,
           before, after, before + enabled, context_ok,
           context_ok && reason == (enabled ? 4UL : 0UL) ? "PASS" : "FAIL");
    if (slot >= 0 && slot < 16)
        *cleanup_ok = dasics_libcfg_free(slot) == 0 &&
            !(csr_read(0x880) & (DASICS_LIBCFG_V << (slot * 4)));
    else
        *cleanup_ok = 0;
    return failures;
}

static int control_sections(const struct control_state *initial, unsigned long *total)
{
    struct control_observation marker = {0}, freezone = {0};
    unsigned long before = dasics_n_extension_trap_count;
    int failures = 0;
    csr_write(0x8b3, 0);
    csr_write(CSR_USCRATCH, 0);
    control_invoke(&marker, control_section_ulib_entry, NULL, NULL);
    int marker_ok = dasics_n_extension_trap_count == before &&
        marker.return_pc == (unsigned long)control_bound_return && marker.freason == 0;
    failures += control_value("ELF-ULIB-CALL-TARGET-LOAD", marker.marker,
                               CONTROL_ULIB_MARKER, marker_ok, total);
    int loaded = (initial->jump_cfg & 1UL) &&
        initial->jump[0] <= (unsigned long)control_jump_target &&
        (unsigned long)control_jump_target < initial->jump[1];
    failures += control_value("ELF-FREEZONE-BOUND-LOAD", loaded, 1, 1, total);
    printf("CONTROL_ELF_INITIAL source=0x%lx target=0x%lx slot=0 cfg=0x%lx "
           "lo=0x%lx hi=0x%lx result=%s\n",
           (unsigned long)control_jal_entry, (unsigned long)control_jump_target,
           initial->jump_cfg, initial->jump[0], initial->jump[1], loaded ? "PASS" : "FAIL");
    csr_write(CSR_USCRATCH, (unsigned long)control_jal_recovery);
    control_invoke(&freezone, control_jal_entry, control_jump_target, control_jal_recovery);
    int path_ok = loaded && dasics_n_extension_trap_count == before &&
        freezone.hits == 1 && freezone.target_pc == (unsigned long)control_jump_target &&
        freezone.ra == (unsigned long)control_jal_site + 4 &&
        freezone.t2 == CONTROL_T2_SEED && freezone.freason == 0 &&
        freezone.return_pc == (unsigned long)control_bound_return &&
        freezone.active == (unsigned long)control_jal_recovery &&
        freezone.uscratch == (unsigned long)control_jal_recovery &&
        csr_read(CSR_USCRATCH) == 0;
    failures += control_value("ELF-UMAIN-ULIB-FREEZONE-CALL", freezone.hits, 1, path_ok, total);
    printf("CONTROL_ELF_PATH source=0x%lx site=0x%lx target=0x%lx target_pc=0x%lx "
           "recovery=0x%lx rd=0x%lx rd_expected=0x%lx returnpc=0x%lx "
           "u24=%lu u24_expected=%lu result=%s\n",
           (unsigned long)control_jal_entry, (unsigned long)control_jal_site,
           (unsigned long)control_jump_target, freezone.target_pc,
           (unsigned long)control_jal_recovery, freezone.ra, (unsigned long)control_jal_site + 4,
           freezone.return_pc, dasics_n_extension_trap_count, before, path_ok ? "PASS" : "FAIL");
    return failures;
}

int main(int argc, char *argv[])
{
    const char *scenario = NULL;
    int enabled = 1, failures = 0, cleanup_ok = 1;
    unsigned long expected_total, expected_traps, total = 0;
    if (argc == 2 && !strcmp(argv[1], "--scenario=direct")) {
        scenario = "direct"; expected_total = 34; expected_traps = 18;
    } else if (argc == 2 && !strcmp(argv[1], "--scenario=special")) {
        scenario = "special"; expected_total = 12; expected_traps = 6;
    } else if (argc == 2 && !strcmp(argv[1], "--scenario=sections")) {
        scenario = "sections"; expected_total = 6; expected_traps = 0;
    } else if (argc == 3 && !strcmp(argv[1], "--scenario=library") &&
               (!strcmp(argv[2], "--expect=off") || !strcmp(argv[2], "--expect=on"))) {
        scenario = "library"; enabled = !strcmp(argv[2], "--expect=on");
        expected_total = enabled ? 6 : 5; expected_traps = enabled;
    } else {
        fprintf(stderr, CONTROL_TAG " argument-contract result=FAIL\n");
        return 2;
    }
    struct control_state initial = control_read_state();
    unsigned long initial_status = csr_read(CSR_USTATUS);
    unsigned long cfg = dasics_linux_query_umaincfg();
    int profile_ok = cfg == (enabled ? 2UL : 0UL) &&
        dasics_n_extension_trap_count == 0 && (initial_status & 1UL) &&
        csr_read(CSR_UTVEC) == (unsigned long)dasics_ufault_entry;
    printf(CONTROL_TAG " begin version=1 scenario=%s enabled=%d expected_total=%lu expected_u24=%lu\n",
           scenario, enabled, expected_total, expected_traps);
    failures += control_value("PROFILE", cfg, enabled ? 2UL : 0UL, profile_ok, &total);
    if (profile_ok) {
        if (!strcmp(scenario, "direct"))
            failures += control_direct(&total);
        else if (!strcmp(scenario, "special"))
            failures += control_special(&total);
        else if (!strcmp(scenario, "sections"))
            failures += control_sections(&initial, &total);
        else
            failures += control_library(enabled, &total, &cleanup_ok);
    }
    int restored = control_restore_state(&initial);
    failures += control_value("STATE-RESTORE", restored, 1, cleanup_ok, &total);
    unsigned long traps = dasics_n_extension_trap_count;
    unsigned long status = csr_read(CSR_USTATUS);
    unsigned long expected_status = initial_status | (expected_traps ? 0x10UL : 0UL);
    int runtime_ok = traps == expected_traps && traps <= DASICS_N_EXTENSION_TRAP_RECORDS &&
        status == expected_status && dasics_n_extension_syscall_trap_count == 0;
    failures += control_value("N-RUNTIME-GUARD", traps, expected_traps, runtime_ok, &total);
    printf("CONTROL_RUNTIME_OBSERVATION scenario=%s ustatus=0x%lx ustatus_expected=0x%lx "
           "u24=%lu u24_expected=%lu syscall_u24=%lu syscall_u24_expected=0 result=%s\n",
           scenario, status, expected_status, traps, expected_traps,
           dasics_n_extension_syscall_trap_count, runtime_ok ? "PASS" : "FAIL");
    if (total != expected_total)
        failures++;
    printf(CONTROL_TAG " summary version=1 scenario=%s enabled=%d total=%lu failed=%d result=%s\n",
           scenario, enabled, total, failures, failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
#else
int main(void)
{
    fprintf(stderr, CONTROL_TAG " requires Linux N-profile result=FAIL\n");
    return 2;
}
#endif
