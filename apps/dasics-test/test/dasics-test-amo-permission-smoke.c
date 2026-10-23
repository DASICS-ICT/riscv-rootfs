#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ucsr.h"
#include "udasics.h"

#define DASICS_AMO_TAG "\033[1;34m[DASICS-AMO-PERMISSION]\033[0m"
#define DASICS_AMO_BEGIN_TAG "\033[1;31m[DASICS-AMO-PERMISSION]"
#define DASICS_AMO_SUMMARY_TAG "\033[1;32m[DASICS-AMO-PERMISSION]"
#define DASICS_AMO_COLOR_END "\033[0m"

#ifndef DASICS_N_EXTENSION_PROFILE
int main(void)
{
    fprintf(stderr, DASICS_AMO_TAG " N extension runtime required result=FAIL\n");
    return 2;
}
#else

#define AMO_GPR_SEED 0xfeedfacecafebeefUL
#define AMO_INITIAL 0x8000000080000021UL
#define AMO_OPERAND 0x7fffffff00000003UL
#define AMO_SC_ATTEMPTS 128UL
#define AMO_EXPECTED_FAULTS 28UL
#define STR_IMPL(value) #value
#define XSTR(value) STR_IMPL(value)

/* Every address, including rejected endpoints, stays in this allocated line.
 * Bytes outside the instruction's own width detect unintended writes. */
static volatile unsigned char amo_arena[64] __attribute__((aligned(64)));

enum amo_operation {
    OP_LR, OP_SC, OP_ADD, OP_SWAP, OP_XOR, OP_AND, OP_OR,
    OP_MIN, OP_MAX, OP_MINU, OP_MAXU
};

enum amo_permission {
    PERM_NONE, PERM_V_ONLY, PERM_R_ONLY, PERM_W_ONLY,
    PERM_RW, PERM_INVALID_RW, PERM_SPLIT_RW
};

struct amo_probe {
    const char *name;
    enum amo_operation operation;
    char *entry, *site, *recovery;
};

struct amo_return {
    unsigned long gpr, return_pc;
};

/* t2 is the actual atomic destination, seeded before the faulting word. The
 * recovery instruction returns t2, rather than an unrelated a0 marker. */
#define DEFINE_AMO_PROBE(name, size, funct7, source) \
extern char name##_entry[], name##_site[], name##_recovery[]; \
asm(".option push\n.option norvc\n.option norelax\n" \
    ".section .ulibtext,\"ax\",@progbits\n.balign 8\n" \
    ".global " #name "_entry\n.type " #name "_entry,@function\n" \
    #name "_entry:\nmv t0,a1\nmv t1,a2\nli t2,0xfeedfacecafebeef\n" \
    ".global " #name "_site\n" #name "_site:\n" \
    ".insn r 0x2f," XSTR(size) "," XSTR(funct7) ",t2,t0," #source "\n" \
    ".global " #name "_recovery\n" #name "_recovery:\nmv a0,t2\nret\n" \
    ".size " #name "_entry,.-" #name "_entry\n" \
    ".section .text,\"ax\",@progbits\n.option pop\n");

#define DEFINE_AMO_WIDTHS(op, funct7, source) \
    DEFINE_AMO_PROBE(amo_w_##op, 2, funct7, source) \
    DEFINE_AMO_PROBE(amo_d_##op, 3, funct7, source)

DEFINE_AMO_WIDTHS(lr, 8, x0)
DEFINE_AMO_WIDTHS(sc, 12, t1)
DEFINE_AMO_WIDTHS(add, 0, t1)
DEFINE_AMO_WIDTHS(swap, 4, t1)
DEFINE_AMO_WIDTHS(xor, 16, t1)
DEFINE_AMO_WIDTHS(and, 48, t1)
DEFINE_AMO_WIDTHS(or, 32, t1)
DEFINE_AMO_WIDTHS(min, 64, t1)
DEFINE_AMO_WIDTHS(max, 80, t1)
DEFINE_AMO_WIDTHS(minu, 96, t1)
DEFINE_AMO_WIDTHS(maxu, 112, t1)

#define PROBE_ROW(prefix, suffix, op, label) \
    {label, op, prefix##_##suffix##_entry, prefix##_##suffix##_site, prefix##_##suffix##_recovery}
#define PROBE_TABLE(prefix) \
    PROBE_ROW(prefix, lr, OP_LR, "LR"), PROBE_ROW(prefix, sc, OP_SC, "SC"), \
    PROBE_ROW(prefix, add, OP_ADD, "ADD"), PROBE_ROW(prefix, swap, OP_SWAP, "SWAP"), \
    PROBE_ROW(prefix, xor, OP_XOR, "XOR"), PROBE_ROW(prefix, and, OP_AND, "AND"), \
    PROBE_ROW(prefix, or, OP_OR, "OR"), PROBE_ROW(prefix, min, OP_MIN, "MIN"), \
    PROBE_ROW(prefix, max, OP_MAX, "MAX"), PROBE_ROW(prefix, minu, OP_MINU, "MINU"), \
    PROBE_ROW(prefix, maxu, OP_MAXU, "MAXU")

static const struct amo_probe amo_word_probes[] = { PROBE_TABLE(amo_w) };
static const struct amo_probe amo_double_probes[] = { PROBE_TABLE(amo_d) };

extern struct amo_return amo_invoke(char *entry, volatile unsigned char *address,
                                    unsigned long operand, unsigned long reserve,
                                    unsigned long width);
extern char amo_probe_return[];

/* Save the caller before LR. Between trusted LR and untrusted SC there are
 * only register/control operations; an interrupt may still lose reservation.
 * a0/a1 return the observed destination and the wrapper's return label. */
asm(".option push\n.option norvc\n.option norelax\n"
    ".section .text,\"ax\",@progbits\n.balign 8\n"
    ".global amo_invoke\n.type amo_invoke,@function\namo_invoke:\n"
    "addi sp,sp,-16\nsd ra,8(sp)\nbeqz a3,2f\n"
    "li t0,32\nbeq a4,t0,1f\n"
    ".insn r 0x2f,3,8,t1,a1,x0\nj 2f\n"
    "1:\n.insn r 0x2f,2,8,t1,a1,x0\n"
    "2:\n.word 0x000510fb\n"
    ".global amo_probe_return\namo_probe_return:\n"
    "csrw 0x040,zero\nla a1,amo_probe_return\n"
    "ld ra,8(sp)\naddi sp,sp,16\nret\n"
    ".size amo_invoke,.-amo_invoke\n.option pop\n");

struct amo_saved_state {
    unsigned long cfg, lo0, hi0, lo1, hi1, return_pc, freason, uscratch;
};

static void save_state(struct amo_saved_state *state)
{
    state->cfg = csr_read(0x880);
    state->lo0 = csr_read(0x890);
    state->hi0 = csr_read(0x891);
    state->lo1 = csr_read(0x892);
    state->hi1 = csr_read(0x893);
    state->return_pc = csr_read(0x8b1);
    state->freason = csr_read(0x8b3);
    state->uscratch = csr_read(CSR_USCRATCH);
}

static int restore_state(const struct amo_saved_state *state)
{
    csr_write(0x880, 0);
    csr_write(0x890, state->lo0);
    csr_write(0x891, state->hi0);
    csr_write(0x892, state->lo1);
    csr_write(0x893, state->hi1);
    csr_write(0x8b1, state->return_pc);
    csr_write(0x8b3, state->freason);
    csr_write(CSR_USCRATCH, state->uscratch);
    csr_write(0x880, state->cfg);
    return csr_read(0x880) == state->cfg && csr_read(0x890) == state->lo0 &&
        csr_read(0x891) == state->hi0 && csr_read(0x892) == state->lo1 &&
        csr_read(0x893) == state->hi1 && csr_read(0x8b1) == state->return_pc &&
        csr_read(0x8b3) == state->freason && csr_read(CSR_USCRATCH) == state->uscratch;
}

static unsigned long configure_permissions(enum amo_permission permission,
                                            unsigned long lo, unsigned long hi)
{
    static const unsigned long configurations[] = {0, 8, 10, 9, 11, 3, 0x9a};
    unsigned long cfg = configurations[permission];
    csr_write(0x880, 0);
    csr_write(0x890, lo);
    csr_write(0x891, hi);
    csr_write(0x892, permission == PERM_SPLIT_RW ? lo : 0);
    csr_write(0x893, permission == PERM_SPLIT_RW ? hi : 0);
    csr_write(0x8b3, 0);
    csr_write(0x880, cfg);
    return cfg;
}

static int permission_allows(enum amo_operation operation, enum amo_permission permission)
{
    if (permission == PERM_RW)
        return 1;
    if (operation == OP_LR)
        return permission == PERM_R_ONLY || permission == PERM_SPLIT_RW;
    if (operation == OP_SC)
        return permission == PERM_W_ONLY || permission == PERM_SPLIT_RW;
    return 0;
}

static unsigned long width_mask(unsigned int width)
{
    return width == 32 ? 0xffffffffUL : ~0UL;
}

static unsigned long extended_old(unsigned long value, unsigned int width)
{
    return width == 32 ? (unsigned long)(long)(int32_t)value : value;
}

static unsigned long expected_memory(enum amo_operation operation, unsigned int width)
{
    unsigned long initial = AMO_INITIAL & width_mask(width);
    unsigned long operand = AMO_OPERAND & width_mask(width);
    int less = width == 32 ? (int32_t)initial < (int32_t)operand :
                            (int64_t)initial < (int64_t)operand;
    switch (operation) {
    case OP_LR: return initial;
    case OP_SC: case OP_SWAP: return operand;
    case OP_ADD: return (initial + operand) & width_mask(width);
    case OP_XOR: return initial ^ operand;
    case OP_AND: return initial & operand;
    case OP_OR: return initial | operand;
    case OP_MIN: return less ? initial : operand;
    case OP_MAX: return less ? operand : initial;
    case OP_MINU: return initial < operand ? initial : operand;
    case OP_MAXU: return initial < operand ? operand : initial;
    }
    return 0;
}

static unsigned long read_memory(unsigned int offset, unsigned int bytes)
{
    unsigned long value = 0;
    for (unsigned int i = 0; i < bytes; i++)
        value |= (unsigned long)amo_arena[offset + i] << (i * 8);
    return value;
}

static void prepare_memory(unsigned int offset, unsigned int bytes)
{
    for (unsigned int i = 0; i < sizeof(amo_arena); i++)
        amo_arena[i] = 0x40 + i;
    for (unsigned int i = 0; i < bytes; i++)
        amo_arena[offset + i] = AMO_INITIAL >> (i * 8);
}

static int arena_matches(unsigned int offset, unsigned int bytes, unsigned long value)
{
    int pass = 1;
    for (unsigned int i = 0; i < sizeof(amo_arena); i++) {
        unsigned char expected = i >= offset && i < offset + bytes ?
            value >> ((i - offset) * 8) : 0x40 + i;
        pass = pass && amo_arena[i] == expected;
    }
    return pass;
}

static int run_case(const struct amo_probe *probe, unsigned int width,
                    enum amo_permission permission, const char *name,
                    unsigned int offset, unsigned int bound_bytes, int inside,
                    unsigned long *total)
{
    unsigned int bytes = width / 8;
    int allow = inside && permission_allows(probe->operation, permission);
    int sc = probe->operation == OP_SC;
    unsigned long reason_expected = allow ? 0 : probe->operation == OP_LR ? 2 : 3;
    unsigned long gpr_expected = !allow ? AMO_GPR_SEED : sc ? 0 :
        extended_old(AMO_INITIAL, width);
    unsigned long memory_expected = allow ? expected_memory(probe->operation, width) :
        AMO_INITIAL & width_mask(width);
    unsigned long lo = (unsigned long)&amo_arena[16], hi = lo + bound_bytes;
    unsigned long address = (unsigned long)&amo_arena[offset];
    unsigned long before = dasics_n_extension_trap_count, after, reason, attempts = 0;
    unsigned long cause = 0, epc = 0, tval = 0, recovery = 0, trap_gpr = 0, trap_scratch = 0;
    struct amo_return observed = {0};
    prepare_memory(offset, bytes);
    unsigned long cfg = configure_permissions(permission, lo, hi);
    int configured = csr_read(0x880) == cfg && csr_read(0x890) == lo && csr_read(0x891) == hi &&
        csr_read(0x892) == (permission == PERM_SPLIT_RW ? lo : 0) &&
        csr_read(0x893) == (permission == PERM_SPLIT_RW ? hi : 0);

    /* Reservation failure is retried only when SC itself returned status 1,
     * no FDI exception occurred, and the entire arena remained unchanged. */
    do {
        csr_write(CSR_USCRATCH, (unsigned long)probe->recovery);
        observed = amo_invoke(probe->entry, &amo_arena[offset], AMO_OPERAND, sc, width);
        attempts++;
        after = dasics_n_extension_trap_count;
        if (!sc || !allow || observed.gpr != 1 || after != before ||
            !arena_matches(offset, bytes, AMO_INITIAL & width_mask(width)))
            break;
    } while (attempts < AMO_SC_ATTEMPTS);

    reason = csr_read(0x8b3);
    unsigned long memory = read_memory(offset, bytes);
    unsigned long return_pc = csr_read(0x8b1), scratch = csr_read(CSR_USCRATCH);
    int arena_ok = arena_matches(offset, bytes, memory_expected);
    int trap_ok = after == before + !allow;
    if (!allow) {
        if (before >= DASICS_N_EXTENSION_TRAP_RECORDS || after != before + 1) {
            trap_ok = 0;
        } else {
            const volatile dasics_n_extension_trap_record_t *record =
                &dasics_n_extension_trap_records[before];
            cause = record->ucause;
            epc = record->uepc;
            tval = record->utval;
            recovery = record->recovery;
            trap_gpr = record->gpr[7];
            trap_scratch = record->uscratch;
            trap_ok = trap_ok && cause == 24 && epc == (unsigned long)probe->site &&
                tval == address && recovery == (unsigned long)probe->recovery &&
                trap_scratch == (unsigned long)probe->recovery &&
                trap_gpr == AMO_GPR_SEED && record->dfreason == reason_expected &&
                record->gpr[5] == address &&
                (record->ustatus & 0x11UL) == 0x10UL;
        }
    }
    int permission_ok = configured && trap_ok && reason == reason_expected;
    int reservation_success = sc && allow && observed.gpr == 0;
    int pass = permission_ok && observed.gpr == gpr_expected && memory == memory_expected &&
        arena_ok && return_pc == (unsigned long)amo_probe_return &&
        observed.return_pc == (unsigned long)amo_probe_return && scratch == 0 &&
        reservation_success == (sc && allow);
    (*total)++;
    printf(DASICS_AMO_TAG " case=AMO%u-%s-%s value=0x%lx expect=0x1 "
           "addr=0x%lx gpr=0x%lx gpr_expected=0x%lx memory=0x%lx memory_expected=0x%lx "
           "arena_ok=%d arena_expected=1 cfg=0x%lx cfg_expected=0x%lx "
           "freason=0x%lx freason_expected=0x%lx traps=%lu->%lu trap_delta_expected=%d "
           "cause=0x%lx cause_expected=0x%lx epc=0x%lx epc_expected=0x%lx "
           "tval=0x%lx tval_expected=0x%lx recovery=0x%lx recovery_expected=0x%lx "
           "trap_gpr=0x%lx trap_gpr_expected=0x%lx trap_uscratch=0x%lx trap_uscratch_expected=0x%lx "
           "returnpc=0x%lx returnpc_expected=0x%lx uscratch=0x%lx uscratch_expected=0x0 "
           "permission_ok=%d permission_expected=1 reservation_success=%d reservation_expected=%d "
           "attempts=%lu reservation_exhausted=%d result=%s\n",
           width, probe->name, name, (unsigned long)pass, address, observed.gpr, gpr_expected,
           memory, memory_expected, arena_ok, csr_read(0x880), cfg, reason, reason_expected,
           before, after, !allow, cause, allow ? 0UL : 24UL,
           epc, allow ? 0UL : (unsigned long)probe->site, tval, allow ? 0UL : address,
           recovery, allow ? 0UL : (unsigned long)probe->recovery,
           trap_gpr, allow ? 0UL : AMO_GPR_SEED,
           trap_scratch, allow ? 0UL : (unsigned long)probe->recovery,
           return_pc, (unsigned long)amo_probe_return, scratch, permission_ok,
           reservation_success, sc && allow, attempts,
           sc && allow && observed.gpr == 1 && attempts == AMO_SC_ATTEMPTS,
           pass ? "PASS" : "FAIL");
    return !pass;
}

int main(int argc, char *argv[])
{
    static const char *const permissions[] = {
        "NONE", "V-ONLY", "R-ONLY", "W-ONLY", "RW", "INVALID-RW", "SPLIT-RW"
    };
    unsigned int width = 64;
    unsigned long total = 0;
    int failures = 0;
    struct amo_saved_state saved;
    if (argc == 2 && !strcmp(argv[1], "--width=32"))
        width = 32;
    else if (argc != 1 && !(argc == 2 && !strcmp(argv[1], "--width=64"))) {
        fprintf(stderr, DASICS_AMO_TAG " usage: [--width=32|--width=64] -dasics result=FAIL\n");
        return 2;
    }
    const struct amo_probe *probes = width == 32 ? amo_word_probes : amo_double_probes;
    unsigned long cfg = dasics_linux_query_umaincfg();
    int profile_ok = cfg == 2 && dasics_n_extension_trap_count == 0 &&
        DASICS_N_EXTENSION_TRAP_RECORDS >= AMO_EXPECTED_FAULTS;
    printf(DASICS_AMO_BEGIN_TAG " amo permission smoke begin width=%u" DASICS_AMO_COLOR_END "\n", width);
    total++;
    failures += !profile_ok;
    printf(DASICS_AMO_TAG " case=AMO%u-PROFILE value=0x%lx expect=0x1 "
           "umaincfg=0x%lx umaincfg_expected=0x2 traps=%lu traps_expected=0 "
           "record_capacity=%lu record_capacity_min=%lu result=%s\n",
           width, (unsigned long)profile_ok, cfg, dasics_n_extension_trap_count,
           DASICS_N_EXTENSION_TRAP_RECORDS, AMO_EXPECTED_FAULTS, profile_ok ? "PASS" : "FAIL");
    if (profile_ok) {
        save_state(&saved);
        for (unsigned int op = OP_LR; op <= OP_ADD; op++)
            for (unsigned int permission = PERM_NONE; permission <= PERM_SPLIT_RW; permission++)
                failures += run_case(&probes[op], width, permission, permissions[permission],
                                     16, 8, 1, &total);
        for (unsigned int op = OP_SWAP; op <= OP_MAXU; op++) {
            failures += run_case(&probes[op], width, PERM_RW, "RW", 16, 8, 1, &total);
            failures += run_case(&probes[op], width, PERM_R_ONLY, "R-ONLY", 16, 8, 1, &total);
        }
        for (unsigned int op = OP_LR; op <= OP_ADD; op++) {
            failures += run_case(&probes[op], width, PERM_RW, "LO-MINUS-WIDTH",
                                 16 - width / 8, 16, 0, &total);
            failures += run_case(&probes[op], width, PERM_RW, "LO", 16, 16, 1, &total);
            failures += run_case(&probes[op], width, PERM_RW, "HI-MINUS-WIDTH",
                                 32 - width / 8, 16, 1, &total);
            failures += run_case(&probes[op], width, PERM_RW, "HI", 32, 16, 0, &total);
        }
        int restored = restore_state(&saved);
        total++;
        failures += !restored;
        printf(DASICS_AMO_TAG " case=AMO%u-STATE-RESTORE value=0x%lx expect=0x1 "
               "cfg=0x%lx cfg_expected=0x%lx returnpc=0x%lx returnpc_expected=0x%lx "
               "freason=0x%lx freason_expected=0x%lx uscratch=0x%lx uscratch_expected=0x%lx result=%s\n",
               width, (unsigned long)restored, csr_read(0x880), saved.cfg,
               csr_read(0x8b1), saved.return_pc, csr_read(0x8b3), saved.freason,
               csr_read(CSR_USCRATCH), saved.uscratch, restored ? "PASS" : "FAIL");
    }
    printf(DASICS_AMO_SUMMARY_TAG " summary total=%lu failed=%d result=%s" DASICS_AMO_COLOR_END "\n",
           total, failures, failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
#endif
