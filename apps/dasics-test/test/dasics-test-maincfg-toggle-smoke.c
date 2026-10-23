#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef DASICS_N_EXTENSION_PROFILE
#include "udasics.h"
#endif
#if defined(DASICS_N_EXTENSION_PROFILE) && DASICS_LINUX_DUAL_EXEC && DASICS_VALIDATION_PROFILE
#include "fdi_verify_client.h"
#define MAINCFG_EXT_AVAILABLE 1
#else
#define MAINCFG_EXT_AVAILABLE 0
#endif

#define STR_IMPL(value) #value
#define XSTR(value) STR_IMPL(value)

#define MAINCFG_TOGGLE_MAGIC 0x4644494d43464755UL
#define MAINCFG_TOGGLE_ARM 0x41524dUL
#define MAINCFG_TOGGLE_REPORT 0x525054UL
#define MAINCFG_TOGGLE_HU_REPORT 0x4855525054UL
#define MAINCFG_TOGGLE_GETPID 306UL
/* Match the kernel profile: four stages for each controlled operation. */
#define MAINCFG_TOGGLE_OBSERVATIONS 16UL

#define MAINCFG_TOGGLE_ECALL_SENTINEL 0x4543414c4c534954UL
#define MAINCFG_TOGGLE_ECALL_GUARD 0x47554152444d4346UL
#define MAINCFG_TOGGLE_LOAD_RD_SENTINEL 0x5a5a5a5a5a5a5a5aUL
#define MAINCFG_TOGGLE_LOAD_VALUE 0x554c4f4144444154UL
#define MAINCFG_TOGGLE_STORE_ATTEMPT 0x5553544f52454421UL
#define MAINCFG_TOGGLE_JUMP_LINK_SENTINEL 0x1122334455667788UL
#define MAINCFG_TOGGLE_JUMP_INITIAL 0x554a554d50494e49UL
#define MAINCFG_TOGGLE_JUMP_TAKEN 0x554a554d5054414bUL

enum maincfg_toggle_operation {
    MAINCFG_TOGGLE_ECALL,
    MAINCFG_TOGGLE_LOAD,
    MAINCFG_TOGGLE_STORE,
    MAINCFG_TOGGLE_JUMP,
};

extern char maincfg_toggle_ecall_site[];
extern char maincfg_toggle_load_site[];
extern char maincfg_toggle_store_site[];
extern char maincfg_toggle_jump_site[];
extern char maincfg_toggle_ecall_recovery[];
extern char maincfg_toggle_load_recovery[];
extern char maincfg_toggle_store_recovery[];
extern char maincfg_toggle_jump_recovery[];
extern char maincfg_toggle_jump_target[];

extern uint64_t maincfg_toggle_ecall_invoke(void);
extern uint64_t maincfg_toggle_load_invoke(const volatile uint64_t *address);
extern uint64_t maincfg_toggle_store_invoke(volatile uint64_t *address,
                                            uint64_t value);
extern void maincfg_toggle_jump_invoke(uint64_t target, uint64_t *link,
                                       uint64_t *marker);

static volatile uint64_t maincfg_toggle_ecall_guard = MAINCFG_TOGGLE_ECALL_GUARD;
static volatile uint64_t maincfg_toggle_load_data = MAINCFG_TOGGLE_LOAD_VALUE;
static volatile uint64_t maincfg_toggle_store_data;
static char *const maincfg_toggle_sites[] = {
    maincfg_toggle_ecall_site,
    maincfg_toggle_load_site,
    maincfg_toggle_store_site,
    maincfg_toggle_jump_site,
};
static char *const maincfg_toggle_recoveries[] = {
    maincfg_toggle_ecall_recovery,
    maincfg_toggle_load_recovery,
    maincfg_toggle_store_recovery,
    maincfg_toggle_jump_recovery,
};
static const volatile uint64_t *const maincfg_toggle_operands[] = {
    &maincfg_toggle_ecall_guard,
    &maincfg_toggle_load_data,
    &maincfg_toggle_store_data,
    (const volatile uint64_t *)maincfg_toggle_jump_target,
};

asm(
    ".option push\n"
    ".option norvc\n"
    ".section .text,\"ax\",@progbits\n"
    ".balign 8\n"
    ".global maincfg_toggle_ecall_invoke\n"
    ".type maincfg_toggle_ecall_invoke, @function\n"
    "maincfg_toggle_ecall_invoke:\n"
    "  addi sp, sp, -16\n"
    "  sd ra, 8(sp)\n"
    "  li a0, " XSTR(MAINCFG_TOGGLE_ECALL_SENTINEL) "\n"
    "  li a7, " XSTR(MAINCFG_TOGGLE_GETPID) "\n"
    "  la t0, maincfg_toggle_ecall_source\n"
    ".global maincfg_toggle_ecall_entry_jalr\n"
    "maincfg_toggle_ecall_entry_jalr:\n"
    "  jalr ra, 0(t0)\n"
    ".global maincfg_toggle_ecall_recovery\n"
    "maincfg_toggle_ecall_recovery:\n"
    "  ld ra, 8(sp)\n"
    "  addi sp, sp, 16\n"
    "  ret\n"
    ".size maincfg_toggle_ecall_invoke, .-maincfg_toggle_ecall_invoke\n"

    ".balign 8\n"
    ".global maincfg_toggle_load_invoke\n"
    ".type maincfg_toggle_load_invoke, @function\n"
    "maincfg_toggle_load_invoke:\n"
    "  addi sp, sp, -16\n"
    "  sd ra, 8(sp)\n"
    "  li t2, " XSTR(MAINCFG_TOGGLE_LOAD_RD_SENTINEL) "\n"
    "  la t0, maincfg_toggle_load_source\n"
    ".global maincfg_toggle_load_entry_jalr\n"
    "maincfg_toggle_load_entry_jalr:\n"
    "  jalr ra, 0(t0)\n"
    ".global maincfg_toggle_load_recovery\n"
    "maincfg_toggle_load_recovery:\n"
    "  mv a0, t2\n"
    "  ld ra, 8(sp)\n"
    "  addi sp, sp, 16\n"
    "  ret\n"
    ".size maincfg_toggle_load_invoke, .-maincfg_toggle_load_invoke\n"

    ".balign 8\n"
    ".global maincfg_toggle_store_invoke\n"
    ".type maincfg_toggle_store_invoke, @function\n"
    "maincfg_toggle_store_invoke:\n"
    "  addi sp, sp, -16\n"
    "  sd ra, 8(sp)\n"
    "  la t0, maincfg_toggle_store_source\n"
    ".global maincfg_toggle_store_entry_jalr\n"
    "maincfg_toggle_store_entry_jalr:\n"
    "  jalr ra, 0(t0)\n"
    ".global maincfg_toggle_store_recovery\n"
    "maincfg_toggle_store_recovery:\n"
    "  mv a0, a1\n"
    "  ld ra, 8(sp)\n"
    "  addi sp, sp, 16\n"
    "  ret\n"
    ".size maincfg_toggle_store_invoke, .-maincfg_toggle_store_invoke\n"

    ".balign 8\n"
    ".global maincfg_toggle_jump_invoke\n"
    ".type maincfg_toggle_jump_invoke, @function\n"
    "maincfg_toggle_jump_invoke:\n"
    "  addi sp, sp, -32\n"
    "  sd ra, 24(sp)\n"
    "  sd a1, 16(sp)\n"
    "  sd a2, 8(sp)\n"
    "  li t2, " XSTR(MAINCFG_TOGGLE_JUMP_LINK_SENTINEL) "\n"
    "  li t3, " XSTR(MAINCFG_TOGGLE_JUMP_INITIAL) "\n"
    "  la t0, maincfg_toggle_jump_source\n"
    ".global maincfg_toggle_jump_entry_jalr\n"
    "maincfg_toggle_jump_entry_jalr:\n"
    "  jalr ra, 0(t0)\n"
    ".global maincfg_toggle_jump_recovery\n"
    "maincfg_toggle_jump_recovery:\n"
    "  ld a1, 16(sp)\n"
    "  ld a2, 8(sp)\n"
    "  sd t2, 0(a1)\n"
    "  sd t3, 0(a2)\n"
    "  ld ra, 24(sp)\n"
    "  addi sp, sp, 32\n"
    "  ret\n"
    ".size maincfg_toggle_jump_invoke, .-maincfg_toggle_jump_invoke\n"

    ".balign 8\n"
    ".global maincfg_toggle_jump_target\n"
    ".type maincfg_toggle_jump_target, @function\n"
    "maincfg_toggle_jump_target:\n"
    "  li t3, " XSTR(MAINCFG_TOGGLE_JUMP_TAKEN) "\n"
    ".global maincfg_toggle_jump_target_return\n"
    "maincfg_toggle_jump_target_return:\n"
    "  jr t2\n"
    ".size maincfg_toggle_jump_target, .-maincfg_toggle_jump_target\n"

    ".section .ulibtext,\"ax\",@progbits\n"
    ".balign 8\n"
    ".global maincfg_toggle_ecall_source\n"
    "maincfg_toggle_ecall_source:\n"
    ".global maincfg_toggle_ecall_site\n"
    "maincfg_toggle_ecall_site:\n"
    "  ecall\n"
    ".global maincfg_toggle_ecall_return_site\n"
    "maincfg_toggle_ecall_return_site:\n"
    "  jr ra\n"

    ".balign 8\n"
    ".global maincfg_toggle_load_source\n"
    "maincfg_toggle_load_source:\n"
    ".global maincfg_toggle_load_site\n"
    "maincfg_toggle_load_site:\n"
    "  ld t2, 0(a0)\n"
    ".global maincfg_toggle_load_return_site\n"
    "maincfg_toggle_load_return_site:\n"
    "  jr ra\n"

    ".balign 8\n"
    ".global maincfg_toggle_store_source\n"
    "maincfg_toggle_store_source:\n"
    ".global maincfg_toggle_store_site\n"
    "maincfg_toggle_store_site:\n"
    "  sd a1, 0(a0)\n"
    ".global maincfg_toggle_store_return_site\n"
    "maincfg_toggle_store_return_site:\n"
    "  jr ra\n"

    ".balign 8\n"
    ".global maincfg_toggle_jump_source\n"
    "maincfg_toggle_jump_source:\n"
    ".global maincfg_toggle_jump_site\n"
    "maincfg_toggle_jump_site:\n"
    "  jalr t2, 0(a0)\n"
    ".global maincfg_toggle_jump_return_site\n"
    "maincfg_toggle_jump_return_site:\n"
    "  jr ra\n"
    ".section .text,\"ax\",@progbits\n"
    ".option pop\n");

static long maincfg_toggle_getpid(void)
{
    register uint64_t a0 asm("a0") = 0;
    register uint64_t a7 asm("a7") = MAINCFG_TOGGLE_GETPID;

    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return (long)a0;
}

static long maincfg_toggle_control(uint64_t command, uint64_t pid,
                                   uint64_t step, uint64_t arg4,
                                   uint64_t arg5, uint64_t arg6)
{
    register uint64_t a0 asm("a0") = MAINCFG_TOGGLE_MAGIC;
    register uint64_t a1 asm("a1") = command;
    register uint64_t a2 asm("a2") = pid;
    register uint64_t a3 asm("a3") = step;
    register uint64_t a4 asm("a4") = arg4;
    register uint64_t a5 asm("a5") = arg5;
    register uint64_t a6 asm("a6") = arg6;
    register uint64_t a7 asm("a7") = MAINCFG_TOGGLE_GETPID;

    asm volatile("ecall"
                 : "+r"(a0)
                 : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
                   "r"(a6), "r"(a7)
                 : "memory");
    return (long)a0;
}

static uint64_t symbol_address(char *symbol)
{
    return (uint64_t)(uintptr_t)symbol;
}

static int arm_observation(uint64_t pid, uint64_t step,
                           enum maincfg_toggle_operation op)
{
    return maincfg_toggle_control(
               MAINCFG_TOGGLE_ARM, pid, step,
               symbol_address(maincfg_toggle_sites[op]),
               (uint64_t)(uintptr_t)maincfg_toggle_operands[op],
               symbol_address(maincfg_toggle_recoveries[op])) == (long)pid
               ? 0
               : 1;
}

#ifdef DASICS_N_EXTENSION_PROFILE
static int report_hu_fault(uint64_t pid, uint64_t step,
                           uint64_t trap_before)
{
    uint64_t trap_after = dasics_n_extension_trap_count;
    uint64_t packed_cause_reason;
    volatile dasics_n_extension_trap_record_t *record;

    csr_write(CSR_USCRATCH, 0);
    if (trap_after == trap_before) {
        return 0;
    }
    if (trap_before >= DASICS_N_EXTENSION_TRAP_RECORDS ||
        trap_after != trap_before + 1) {
        return 1;
    }

    record = &dasics_n_extension_trap_records[trap_before];
    packed_cause_reason =
        (record->ucause << 32) | (record->dfreason & 0xffffffffUL);
    return maincfg_toggle_control(
               MAINCFG_TOGGLE_HU_REPORT, pid, step, record->uepc,
               record->utval, packed_cause_reason) == (long)pid
               ? 0
               : 1;
}
#endif

static int report_observation(uint64_t pid, uint64_t step, uint64_t side0,
                              uint64_t side1)
{
    return maincfg_toggle_control(MAINCFG_TOGGLE_REPORT, pid, step, side0,
                                  side1, 0) == (long)pid
               ? 0
               : 1;
}

#if MAINCFG_EXT_AVAILABLE
#define MAINCFG_EXT_TOTAL 16UL
#define MAINCFG_EXT_READ_SEED 0xfeedfacecafebeefUL
#define MAINCFG_EXT_LIB_SEED 0x0123456789abcdefUL
#define MAINCFG_EXT_RETURN_SEED 0x33445000UL

struct maincfg_ext_state {
    unsigned long libcfg, return_pc, active, freason, scratch;
    unsigned long guard, load, store;
};
struct maincfg_ext_pair { unsigned long value, hits; };
struct maincfg_ext_context {
    unsigned long total, failed, windows, cfg, status;
    struct maincfg_ext_state initial;
};
extern struct maincfg_ext_pair maincfg_ext_invoke(char *entry);
extern char maincfg_ext_return[];
extern char maincfg_ext_csr_read_entry[], maincfg_ext_csr_read_site[], maincfg_ext_csr_read_recovery[];
extern char maincfg_ext_csr_write_entry[], maincfg_ext_csr_write_site[], maincfg_ext_csr_write_recovery[];
extern char maincfg_ext_call_j_entry[], maincfg_ext_call_j_site[], maincfg_ext_call_j_recovery[];
extern char maincfg_ext_call_j_target[];
extern char maincfg_ext_call_jr_entry[], maincfg_ext_call_jr_site[], maincfg_ext_call_jr_recovery[];
extern char maincfg_ext_call_jr_target[];

/* Each forbidden call has a safe local target if the hardware executes it.
 * CUFT is closed for these probes, so the cleanup jump needs no extra bound. */
asm(
    ".option push\n.option norvc\n.option norelax\n"
    ".section .text,\"ax\",@progbits\n.balign 8\n"
    ".global maincfg_ext_invoke\n.type maincfg_ext_invoke,@function\n"
    "maincfg_ext_invoke:\naddi sp,sp,-16\nsd ra,8(sp)\nmv t0,a0\n"
    "la t1,maincfg_ext_return\ncsrw 0x8b2,t1\njalr ra,0(t0)\n"
    ".global maincfg_ext_return\nmaincfg_ext_return:\n"
    "ld ra,8(sp)\naddi sp,sp,16\nret\n"
    ".size maincfg_ext_invoke,.-maincfg_ext_invoke\n"
    ".section .ulibtext,\"ax\",@progbits\n.balign 8\n"
    ".global maincfg_ext_csr_read_entry\nmaincfg_ext_csr_read_entry:\n"
    "li a0,0xfeedfacecafebeef\nli a1,0\n"
    ".global maincfg_ext_csr_read_site\nmaincfg_ext_csr_read_site:\n.word 0x8b302573\n"
    ".global maincfg_ext_csr_read_recovery\nmaincfg_ext_csr_read_recovery:\nret\n"
    ".balign 8\n.global maincfg_ext_csr_write_entry\nmaincfg_ext_csr_write_entry:\n"
    "li a0,-1\nli a1,0\n"
    ".global maincfg_ext_csr_write_site\nmaincfg_ext_csr_write_site:\n.word 0x88051073\n"
    ".global maincfg_ext_csr_write_recovery\nmaincfg_ext_csr_write_recovery:\nret\n"
    ".balign 8\n.global maincfg_ext_call_j_entry\nmaincfg_ext_call_j_entry:\n"
    "mv t4,ra\nli ra,0x1122334455667788\nli a1,0\n"
    ".global maincfg_ext_call_j_site\nmaincfg_ext_call_j_site:\n.word 0x0080007b\n"
    ".global maincfg_ext_call_j_recovery\nmaincfg_ext_call_j_recovery:\nj 1f\n"
    ".global maincfg_ext_call_j_target\nmaincfg_ext_call_j_target:\nli a1,1\n"
    "1:\nmv a0,ra\nmv ra,t4\nret\n"
    ".balign 8\n.global maincfg_ext_call_jr_entry\nmaincfg_ext_call_jr_entry:\n"
    "mv t4,ra\nli ra,0x1122334455667788\nli a1,0\nla t3,maincfg_ext_call_jr_target\n"
    ".global maincfg_ext_call_jr_site\nmaincfg_ext_call_jr_site:\n.word 0x000e10fb\n"
    ".global maincfg_ext_call_jr_recovery\nmaincfg_ext_call_jr_recovery:\nj 1f\n"
    ".global maincfg_ext_call_jr_target\nmaincfg_ext_call_jr_target:\nli a1,1\n"
    "1:\nmv a0,ra\nmv ra,t4\nret\n"
    ".section .text,\"ax\",@progbits\n.option pop\n");

static const char *const maincfg_ext_ops[] = {"ecall", "load", "store", "jump"};
static const unsigned long maincfg_ext_open[] = {0x3a, 0x2e, 0x36, 0x1e};

static struct maincfg_ext_state maincfg_ext_snapshot(void)
{
    return (struct maincfg_ext_state){csr_read(0x880), csr_read(0x8b1),
        csr_read(0x8b2), csr_read(0x8b3), csr_read(CSR_USCRATCH),
        maincfg_toggle_ecall_guard, maincfg_toggle_load_data, maincfg_toggle_store_data};
}

static int maincfg_ext_equal(const struct maincfg_ext_state *left,
                             const struct maincfg_ext_state *right)
{
    return !memcmp(left, right, sizeof(*left));
}

static int maincfg_ext_restore(const struct maincfg_ext_state *saved)
{
    csr_write(0x880, saved->libcfg);
    csr_write(0x8b1, saved->return_pc);
    csr_write(0x8b2, saved->active);
    csr_write(0x8b3, saved->freason);
    csr_write(CSR_USCRATCH, saved->scratch);
    maincfg_toggle_ecall_guard = saved->guard;
    maincfg_toggle_load_data = saved->load;
    maincfg_toggle_store_data = saved->store;
    struct maincfg_ext_state actual = maincfg_ext_snapshot();
    return maincfg_ext_equal(&actual, saved);
}

static int maincfg_ext_cross(struct maincfg_ext_context *context, unsigned int owner,
                              unsigned int operation, unsigned long pid)
{
    struct maincfg_ext_state saved = maincfg_ext_snapshot();
    unsigned long before = dasics_n_extension_trap_count, status = csr_read(CSR_USTATUS);
    unsigned long side0 = 0, side1 = 0, expected0, expected1;
    unsigned long recovery = symbol_address(maincfg_toggle_recoveries[operation]);
    /* Distinct initial values make a skipped load/store/jump observable.
     * Restore them before the original armed operation uses its own operand. */
    maincfg_toggle_ecall_guard = MAINCFG_TOGGLE_ECALL_GUARD;
    maincfg_toggle_load_data = MAINCFG_TOGGLE_LOAD_VALUE;
    maincfg_toggle_store_data = 0x55494e495453544fUL;
    csr_write(0x8b2, recovery);
    csr_write(CSR_USCRATCH, recovery);
    switch (operation) {
    case MAINCFG_TOGGLE_ECALL:
        side0 = maincfg_toggle_ecall_invoke();
        side1 = maincfg_toggle_ecall_guard;
        expected0 = pid; expected1 = MAINCFG_TOGGLE_ECALL_GUARD;
        break;
    case MAINCFG_TOGGLE_LOAD:
        side0 = maincfg_toggle_load_invoke(&maincfg_toggle_load_data);
        side1 = maincfg_toggle_load_data;
        expected0 = expected1 = MAINCFG_TOGGLE_LOAD_VALUE;
        break;
    case MAINCFG_TOGGLE_STORE:
        side1 = maincfg_toggle_store_invoke(&maincfg_toggle_store_data, MAINCFG_TOGGLE_STORE_ATTEMPT);
        side0 = maincfg_toggle_store_data;
        expected0 = expected1 = MAINCFG_TOGGLE_STORE_ATTEMPT;
        break;
    default:
        maincfg_toggle_jump_invoke(symbol_address(maincfg_toggle_jump_target), &side0, &side1);
        expected0 = symbol_address(maincfg_toggle_jump_site) + 4;
        expected1 = MAINCFG_TOGGLE_JUMP_TAKEN;
        break;
    }
    struct maincfg_ext_state actual = maincfg_ext_snapshot();
    unsigned long after = dasics_n_extension_trap_count, after_status = csr_read(CSR_USTATUS);
    unsigned long cfg = dasics_linux_query_umaincfg();
    unsigned long expected_store = operation == MAINCFG_TOGGLE_STORE ?
        MAINCFG_TOGGLE_STORE_ATTEMPT : 0x55494e495453544fUL;
    int state_ok = actual.libcfg == saved.libcfg && actual.return_pc == saved.return_pc &&
        actual.active == recovery && actual.freason == 5 && actual.scratch == recovery &&
        actual.guard == MAINCFG_TOGGLE_ECALL_GUARD && actual.load == MAINCFG_TOGGLE_LOAD_VALUE &&
        actual.store == expected_store;
    int restored = maincfg_ext_restore(&saved);
    int pass = cfg == maincfg_ext_open[owner] && side0 == expected0 && side1 == expected1 &&
        after == before && after_status == status && state_ok && restored;
    context->total++;
    context->failed += !pass;
    printf("DASICS_MAINCFG_EXT_CASE version=1 case=CROSS-%s-%s pid=%lu cfg=0x%lx expect_cfg=0x%lx "
           "source=0x%lx operand=0x%lx recovery=0x%lx side0=0x%lx expect_side0=0x%lx "
           "side1=0x%lx expect_side1=0x%lx guard=0x%lx expect_guard=0x%lx "
           "load=0x%lx expect_load=0x%lx store=0x%lx expect_store=0x%lx "
           "freason=0x%lx expect_freason=0x5 returnpc=0x%lx expect_returnpc=0x%lx "
           "active=0x%lx expect_active=0x%lx uscratch=0x%lx expect_uscratch=0x%lx "
           "u24_before=%lu u24_after=%lu u24_expected=%lu ustatus=0x%lx expect_ustatus=0x%lx "
           "state_ok=%d restored=%d result=%s\n",
           maincfg_ext_ops[owner], maincfg_ext_ops[operation], pid, cfg, maincfg_ext_open[owner],
           symbol_address(maincfg_toggle_sites[operation]),
           (unsigned long)(uintptr_t)maincfg_toggle_operands[operation], recovery,
           side0, expected0, side1, expected1, actual.guard, MAINCFG_TOGGLE_ECALL_GUARD,
           actual.load, MAINCFG_TOGGLE_LOAD_VALUE, actual.store, expected_store,
           actual.freason, actual.return_pc, saved.return_pc, actual.active, recovery,
           actual.scratch, recovery, before, after, before, after_status, status,
           state_ok, restored, pass ? "PASS" : "FAIL");
    return !pass;
}

static int maincfg_ext_reject(struct maincfg_ext_context *context, unsigned int probe,
                               unsigned long pid)
{
    static const char *const names[] = {"CLOSED-CSR-R", "CLOSED-CSR-W", "CLOSED-CALL-J", "CLOSED-CALL-JR"};
    char *const entries[] = {maincfg_ext_csr_read_entry, maincfg_ext_csr_write_entry,
        maincfg_ext_call_j_entry, maincfg_ext_call_jr_entry};
    char *const sites[] = {maincfg_ext_csr_read_site, maincfg_ext_csr_write_site,
        maincfg_ext_call_j_site, maincfg_ext_call_jr_site};
    const unsigned long words[] = {0x8b302573, 0x88051073, 0x0080007b, 0x000e10fb};
    struct maincfg_ext_state saved = maincfg_ext_snapshot();
    struct fdi_verify_client observer = {.fd = -1};
    struct fdi_verify_result observed = {0};
    struct maincfg_ext_pair pair = {0};
    unsigned long before = dasics_n_extension_trap_count, status = csr_read(CSR_USTATUS);
    unsigned long expected = probe == 0 ? MAINCFG_EXT_READ_SEED : probe == 1 ? ~0UL :
        MAINCFG_TOGGLE_JUMP_LINK_SENTINEL;
    int begin_rc = -1, arm_rc = -1, query_rc = -1, end_rc = -1, error = 0;
    csr_write(0x880, MAINCFG_EXT_LIB_SEED);
    csr_write(0x8b1, MAINCFG_EXT_RETURN_SEED);
    csr_write(0x8b3, 5);
    begin_rc = fdi_verify_begin(&observer, 0);
    if (begin_rc) {
        error = errno;
    } else {
        context->windows++;
        arm_rc = fdi_verify_arm(&observer, probe < 2 ? probe + 1 : FDI_VERIFY_FDICALL, sites[probe]);
        if (arm_rc) {
            error = errno;
        } else {
            pair = maincfg_ext_invoke(entries[probe]);
            query_rc = fdi_verify_query(&observer);
            if (query_rc)
                error = errno;
            else
                observed = observer.observed;
        }
    }
    struct maincfg_ext_state actual = maincfg_ext_snapshot();
    unsigned long after = dasics_n_extension_trap_count, after_status = csr_read(CSR_USTATUS);
    unsigned long cfg = dasics_linux_query_umaincfg();
    /* An opened lease is ended on every ordinary failure path as well.
     * END overwrites the client snapshot; print the retained real QUERY. */
    if (observer.fd >= 0) {
        end_rc = fdi_verify_end(&observer);
        if (end_rc && !error)
            error = errno;
        if (observer.fd == -1)
            context->windows--;
    }
    int observer_ok = !begin_rc && !arm_rc && !query_rc && !end_rc && !error &&
        observed.version == FDI_VERIFY_VERSION && !observed.flags && observed.active == 1 &&
        observed.pid == pid && observed.cookie && observed.kind == (probe < 2 ? probe + 1 : FDI_VERIFY_FDICALL) &&
        observed.site == symbol_address(sites[probe]) && observed.instruction == words[probe] &&
        observed.illegal_count == 1 && observed.cause == 2 && observed.epc == observed.site &&
        observed.freason == 5 && observed.return_pc == MAINCFG_EXT_RETURN_SEED &&
        observed.gpr[0] == 0 && observed.gpr[11] == 0 &&
        (probe < 2 ? observed.gpr[10] == expected &&
            observed.gpr[1] == symbol_address(maincfg_ext_return) :
            observed.gpr[1] == expected && observed.gpr[29] == symbol_address(maincfg_ext_return)) &&
        (probe != 3 || observed.gpr[28] == symbol_address(maincfg_ext_call_jr_target));
    int restored = maincfg_ext_restore(&saved);
    int pass = observer_ok && cfg == 0x3e && pair.value == expected && !pair.hits &&
        actual.libcfg == MAINCFG_EXT_LIB_SEED && actual.return_pc == MAINCFG_EXT_RETURN_SEED &&
        actual.active == symbol_address(maincfg_ext_return) && actual.freason == 5 &&
        actual.scratch == saved.scratch && actual.guard == saved.guard &&
        actual.load == saved.load && actual.store == saved.store &&
        before == after && status == after_status && observer.fd == -1 && restored;
    context->total++;
    context->failed += !pass;
    printf("DASICS_MAINCFG_EXT_CASE version=1 case=%s pid=%lu cfg=0x%lx expect_cfg=0x3e "
           "site=0x%lx value=0x%lx expect=0x%lx hits=%lu expect_hits=0 "
           "libcfg=0x%lx expect_libcfg=0x%lx returnpc=0x%lx expect_returnpc=0x%lx "
           "active=0x%lx expect_active=0x%lx freason=0x%lx expect_freason=0x5 "
           "u24_before=%lu u24_after=%lu u24_expected=%lu ustatus=0x%lx expect_ustatus=0x%lx "
           "begin_rc=%d arm_rc=%d query_rc=%d end_rc=%d errno=%d fd=%d restored=%d result=%s\n",
           names[probe], pid, cfg, symbol_address(sites[probe]), pair.value, expected, pair.hits,
           actual.libcfg, MAINCFG_EXT_LIB_SEED, actual.return_pc, MAINCFG_EXT_RETURN_SEED,
           actual.active, symbol_address(maincfg_ext_return), actual.freason,
           before, after, before, after_status, status, begin_rc, arm_rc, query_rc, end_rc,
           error, observer.fd, restored, pass ? "PASS" : "FAIL");
    if (!query_rc)
        fdi_verify_print(names[probe], &observed);
    return !pass;
}

static int maincfg_ext_finish(struct maincfg_ext_context *context)
{
    struct maincfg_ext_state actual = maincfg_ext_snapshot();
    unsigned long cfg = dasics_linux_query_umaincfg();
    unsigned long traps = dasics_n_extension_trap_count, status = csr_read(CSR_USTATUS);
    /* The original store workload legitimately changes its operand. The
     * kernel owns CSR restoration; this program owns the three data cells. */
    int kernel_restored = cfg == context->cfg &&
        actual.libcfg == context->initial.libcfg &&
        actual.return_pc == context->initial.return_pc && actual.active == context->initial.active &&
        actual.freason == context->initial.freason && actual.scratch == context->initial.scratch;
    int restored = maincfg_ext_restore(&context->initial);
    actual = maincfg_ext_snapshot();
    int pass = kernel_restored && restored && !context->windows &&
        traps == 4 && status == (context->status | 0x10UL) &&
        dasics_n_extension_syscall_trap_count == 1 &&
        dasics_n_extension_syscall_permitted_count == 0 &&
        dasics_n_extension_syscall_denied_count == 1 &&
        context->total == MAINCFG_EXT_TOTAL;
    context->failed += !pass;
    printf("DASICS_MAINCFG_EXT_CLEANUP cfg=0x%lx expect_cfg=0x%lx libcfg=0x%lx expect_libcfg=0x%lx "
           "returnpc=0x%lx expect_returnpc=0x%lx active=0x%lx expect_active=0x%lx "
           "freason=0x%lx expect_freason=0x%lx uscratch=0x%lx expect_uscratch=0x%lx "
           "guard=0x%lx expect_guard=0x%lx load=0x%lx expect_load=0x%lx "
           "store=0x%lx expect_store=0x%lx windows=%lu restored=%d kernel_restored=%d result=%s\n",
           cfg, context->cfg, actual.libcfg, context->initial.libcfg,
           actual.return_pc, context->initial.return_pc, actual.active, context->initial.active,
           actual.freason, context->initial.freason, actual.scratch, context->initial.scratch,
           actual.guard, context->initial.guard, actual.load, context->initial.load,
           actual.store, context->initial.store, context->windows, restored, kernel_restored,
           pass ? "PASS" : "FAIL");
    printf("DASICS_MAINCFG_EXT_SUMMARY version=1 total=%lu failed=%lu u24=%lu "
           "syscalls=%lu permitted=%lu denied=%lu ustatus=0x%lx result=%s\n",
           context->total, context->failed, traps, dasics_n_extension_syscall_trap_count,
           dasics_n_extension_syscall_permitted_count, dasics_n_extension_syscall_denied_count,
           status, context->failed ? "FAIL" : "PASS");
    return !pass;
}
#endif

int main(int argc, char *argv[])
{
    uint64_t step = 0;
    long pid = maincfg_toggle_getpid();
    int failures = 0;
    int extended = argc == 2 && !strcmp(argv[1], "--isa-extended");
    if (argc != 1 && !extended) {
        fprintf(stderr, "DASICS_MAINCFG_TOGGLE_CLIENT result=FAIL detail=argument-contract\n");
        return 2;
    }
#if MAINCFG_EXT_AVAILABLE
    struct maincfg_ext_context extension = {0};
    if (extended) {
        extension.initial = maincfg_ext_snapshot();
        extension.cfg = dasics_linux_query_umaincfg();
        extension.status = csr_read(CSR_USTATUS);
        int profile_ok = extension.cfg == 2 && extension.status == 1 &&
            dasics_n_extension_trap_count == 0 && dasics_n_extension_syscall_trap_count == 0 &&
            dasics_n_extension_syscall_permitted_count == 0 &&
            dasics_n_extension_syscall_denied_count == 0;
        printf("DASICS_MAINCFG_EXT_BEGIN version=1 expected_total=16 pid=%ld cfg=0x%lx "
               "ustatus=0x%lx profile_ok=%d\n", pid, extension.cfg, extension.status, profile_ok);
        if (!profile_ok)
            return 2;
    }
#else
    if (extended) {
        fprintf(stderr, "DASICS_MAINCFG_TOGGLE_CLIENT result=FAIL detail=validation-profile-required\n");
        return 2;
    }
#endif

    printf("DASICS_MAINCFG_TOGGLE_BEGIN version=1 observations=16\n");
    if (pid <= 0) {
        printf("DASICS_MAINCFG_TOGGLE_CLIENT result=FAIL detail=getpid\n");
        return 1;
    }

    for (int op = MAINCFG_TOGGLE_ECALL; op <= MAINCFG_TOGGLE_JUMP; op++) {
        for (int stage = 0; stage < 4; stage++, step++) {
            uint64_t side0 = 0;
            uint64_t side1 = 0;
#ifdef DASICS_N_EXTENSION_PROFILE
            uint64_t trap_before = dasics_n_extension_trap_count;
#endif

            int arm_error = arm_observation((uint64_t)pid, step, op);
            failures += arm_error;
#if MAINCFG_EXT_AVAILABLE
            if (extended && !arm_error && stage == 1)
                for (int other = MAINCFG_TOGGLE_ECALL; other <= MAINCFG_TOGGLE_JUMP; other++)
                    if (other != op)
                        failures += maincfg_ext_cross(&extension, op, other, pid);
            if (extended && !arm_error && step == 2)
                for (unsigned int probe = 0; probe < 4; probe++)
                    failures += maincfg_ext_reject(&extension, probe, pid);
#endif
#ifdef DASICS_N_EXTENSION_PROFILE
            csr_write(CSR_USCRATCH,
                      symbol_address(maincfg_toggle_recoveries[op]));
#endif
            switch (op) {
            case MAINCFG_TOGGLE_ECALL:
                side0 = maincfg_toggle_ecall_invoke();
                side1 = maincfg_toggle_ecall_guard;
                break;
            case MAINCFG_TOGGLE_LOAD:
                side0 = maincfg_toggle_load_invoke(&maincfg_toggle_load_data);
                side1 = maincfg_toggle_load_data;
                break;
            case MAINCFG_TOGGLE_STORE:
                side1 = maincfg_toggle_store_invoke(
                    &maincfg_toggle_store_data, MAINCFG_TOGGLE_STORE_ATTEMPT);
                side0 = maincfg_toggle_store_data;
                break;
            case MAINCFG_TOGGLE_JUMP:
                side0 = MAINCFG_TOGGLE_JUMP_LINK_SENTINEL;
                side1 = MAINCFG_TOGGLE_JUMP_INITIAL;
                maincfg_toggle_jump_invoke(
                    symbol_address(maincfg_toggle_jump_target), &side0, &side1);
                break;
            }
#ifdef DASICS_N_EXTENSION_PROFILE
            failures += report_hu_fault((uint64_t)pid, step, trap_before);
#endif
            failures += report_observation((uint64_t)pid, step, side0, side1);
        }
    }

#if MAINCFG_EXT_AVAILABLE
    if (extended)
        failures += maincfg_ext_finish(&extension);
#endif
    if (step != MAINCFG_TOGGLE_OBSERVATIONS || failures != 0) {
        printf("DASICS_MAINCFG_TOGGLE_CLIENT result=FAIL step=%lu failed=%d\n",
               step, failures);
        return 1;
    }
    return 0;
}
