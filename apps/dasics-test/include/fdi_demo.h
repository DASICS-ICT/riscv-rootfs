#ifndef FDI_DEMO_H
#define FDI_DEMO_H

#include <stdlib.h>
#include <string.h>
#include "udasics.h"

#if !DASICS_LINUX_DUAL_EXEC || !defined(DASICS_N_EXTENSION_PROFILE)
#error "The functional demos require the Linux N-extension profile"
#endif

#define FDI_DEMO_TAG "[FDI-DEMO]"
#define FDI_DEMO_POISON 0xd15ea5ed5a17c0deUL
#define FDI_DEMO_SLOTS(M) \
    M(0) M(1) M(2) M(3) M(4) M(5) M(6) M(7) \
    M(8) M(9) M(10) M(11) M(12) M(13) M(14) M(15)

/* Only this explicitly granted object carries results out of library code. */
struct fdi_demo_observation {
    unsigned long value, read_value, hits, path, link, selected, unchanged;
    unsigned long original_ra, saved_ra, sp_before, sp_after;
    unsigned long service_count, bound_preserved;
};

struct fdi_demo_state {
    unsigned long libcfg, bounds[32], jumpcfg, jumps[8];
    unsigned long maincall, return_pc, active, reason, scratch, vector, helper;
};

struct fdi_demo {
    const char *name;
    enum dasics_linux_exec_mode mode;
    unsigned int function;
    unsigned long total, before;
    int failed, data_slot, observation_slot, jump_slot;
    struct fdi_demo_state original;
    volatile struct fdi_demo_observation *observed;
    unsigned long source, address, site, recovery, target, reason;
    unsigned long fault_ra, fault_t1, fault_t2, fault_t3;
};

extern uint64_t umaincall_helper;
extern int fdi_demo_invoke(unsigned long, int (*)(void));
extern char fdi_demo_off_return[];

/* The protected path reuses the maintained custom3 library call. The plain
 * path has a named continuation so the host can independently check its link. */
asm(
    ".pushsection .text,\"ax\",@progbits\n.option push\n.option norvc\n.option norelax\n"
    ".balign 8\n.global fdi_demo_invoke\n.type fdi_demo_invoke,@function\n"
    "fdi_demo_invoke:\naddi sp,sp,-16\nsd ra,8(sp)\nbeqz a0,1f\n"
    "mv a0,a1\ncall lib_call\nj 2f\n1:\njalr ra,0(a1)\n"
    ".global fdi_demo_off_return\nfdi_demo_off_return:\n"
    "2:\nld ra,8(sp)\naddi sp,sp,16\nret\n"
    ".size fdi_demo_invoke,.-fdi_demo_invoke\n.option pop\n.popsection\n"
);

static struct fdi_demo_state fdi_demo_read_state(void)
{
    struct fdi_demo_state state = {
        .libcfg = csr_read(CSR_DLCFG0), .jumpcfg = csr_read(CSR_DJCFG),
        .jumps = {csr_read(0x8c0), csr_read(0x8c1), csr_read(0x8c2), csr_read(0x8c3),
                  csr_read(0x8c4), csr_read(0x8c5), csr_read(0x8c6), csr_read(0x8c7)},
        .maincall = csr_read(CSR_DMAINCALL), .return_pc = csr_read(CSR_DRETURNPC),
        .active = csr_read(CSR_DFZRETURN), .reason = csr_read(CSR_DFREASON),
        .scratch = csr_read(CSR_USCRATCH), .vector = csr_read(CSR_UTVEC),
        .helper = umaincall_helper
    };
#define FDI_DEMO_READ(index) \
    state.bounds[2 * index] = csr_read(0x890 + 2 * index); \
    state.bounds[2 * index + 1] = csr_read(0x891 + 2 * index);
    FDI_DEMO_SLOTS(FDI_DEMO_READ)
#undef FDI_DEMO_READ
    return state;
}

static int fdi_demo_restore(const struct fdi_demo_state *state)
{
    csr_write(CSR_DLCFG0, 0);
    csr_write(CSR_DJCFG, 0);
#define FDI_DEMO_WRITE(index) \
    csr_write(0x890 + 2 * index, state->bounds[2 * index]); \
    csr_write(0x891 + 2 * index, state->bounds[2 * index + 1]);
    FDI_DEMO_SLOTS(FDI_DEMO_WRITE)
#undef FDI_DEMO_WRITE
    csr_write(0x8c0, state->jumps[0]); csr_write(0x8c1, state->jumps[1]);
    csr_write(0x8c2, state->jumps[2]); csr_write(0x8c3, state->jumps[3]);
    csr_write(0x8c4, state->jumps[4]); csr_write(0x8c5, state->jumps[5]);
    csr_write(0x8c6, state->jumps[6]); csr_write(0x8c7, state->jumps[7]);
    csr_write(CSR_DMAINCALL, state->maincall);
    csr_write(CSR_DRETURNPC, state->return_pc);
    csr_write(CSR_DFZRETURN, state->active);
    csr_write(CSR_DFREASON, state->reason);
    csr_write(CSR_USCRATCH, state->scratch);
    csr_write(CSR_UTVEC, state->vector);
    umaincall_helper = state->helper;
    csr_write(CSR_DLCFG0, state->libcfg);
    csr_write(CSR_DJCFG, state->jumpcfg);
    struct fdi_demo_state actual = fdi_demo_read_state();
    return !memcmp(&actual, state, sizeof(actual));
}

static void fdi_demo_check(struct fdi_demo *demo, const char *name,
                           unsigned long actual, unsigned long expected)
{
    demo->total++;
    demo->failed += actual != expected;
    printf(FDI_DEMO_TAG " case=%s actual=0x%lx expect=0x%lx result=%s\n",
           name, actual, expected, actual == expected ? "PASS" : "FAIL");
}

static int fdi_demo_begin(struct fdi_demo *demo, int argc, char **argv,
                           const char *name, int flexibility)
{
    char *mode_args[] = {argv[0], argc > 1 ? argv[1] : NULL, NULL};
    char *end = NULL;
    long selected = 0;

    memset(demo, 0, sizeof(*demo));
    demo->name = name;
    demo->data_slot = demo->observation_slot = demo->jump_slot = -1;
    if (argc != (flexibility ? 3 : 2) ||
        dasics_linux_parse_exec_mode(2, mode_args, &demo->mode))
        goto arguments;
    if (flexibility) {
        if (strncmp(argv[2], "--function=", 11) || !argv[2][11])
            goto arguments;
        selected = strtol(argv[2] + 11, &end, 10);
        if (*end || selected < 0 || selected > 99)
            goto arguments;
    }
    demo->function = selected;
    demo->original = fdi_demo_read_state();
    demo->before = dasics_n_extension_trap_count;
    printf(FDI_DEMO_TAG " begin demo=%s dasics=%s function=%u\n", name,
           dasics_linux_exec_mode_name(demo->mode), demo->function);
    fdi_demo_check(demo, "EXEC-UMAINCFG", dasics_linux_query_umaincfg(),
                   demo->mode == DASICS_LINUX_EXEC_ON ? DASICS_UCFG_ENA : 0);
    return 0;
arguments:
    fprintf(stderr, FDI_DEMO_TAG " argument-contract demo=%s result=FAIL\n", name);
    return -1;
}

static int fdi_demo_bound_matches(const struct fdi_demo_state *state, int slot,
                                 unsigned long cfg, const void *base, size_t size)
{
    return slot >= 0 && slot < 16 &&
        ((state->libcfg >> (4 * slot)) & 15) == (DASICS_LIBCFG_V | cfg) &&
        state->bounds[2 * slot] == DASICS_BOUND_ALIGN_DOWN(base) &&
        state->bounds[2 * slot + 1] == DASICS_BOUND_ALIGN_UP((uintptr_t)base + size);
}

static int fdi_demo_setup(struct fdi_demo *demo,
                           volatile struct fdi_demo_observation *observed,
                           const void *readonly, size_t size,
                           const void *jump_lo, const void *jump_hi)
{
    demo->observed = observed;
    memset((void *)observed, 0, sizeof(*observed));
    register_udasics(0);
    csr_write(CSR_DJCFG, 0);
    csr_write(CSR_DFZRETURN, 0);
    csr_write(CSR_DFREASON, 0);
    demo->observation_slot = dasics_libcfg_alloc(DASICS_LIBCFG_R | DASICS_LIBCFG_W,
        DASICS_BOUND_ALIGN_DOWN(observed), DASICS_BOUND_ALIGN_UP(observed + 1));
    if (readonly)
        demo->data_slot = dasics_libcfg_alloc(DASICS_LIBCFG_R,
            DASICS_BOUND_ALIGN_DOWN(readonly),
            DASICS_BOUND_ALIGN_UP((uintptr_t)readonly + size));
    if (jump_lo)
        demo->jump_slot = dasics_jumpcfg_alloc(DASICS_BOUND_ALIGN_DOWN(jump_lo),
                                               DASICS_BOUND_ALIGN_UP(jump_hi));
    struct fdi_demo_state state = fdi_demo_read_state();
    int valid = fdi_demo_bound_matches(&state, demo->observation_slot,
        DASICS_LIBCFG_R | DASICS_LIBCFG_W, (const void *)observed, sizeof(*observed));
    if (readonly)
        valid &= fdi_demo_bound_matches(&state, demo->data_slot,
                                         DASICS_LIBCFG_R, readonly, size);
    if (jump_lo)
        valid &= demo->jump_slot >= 0 && demo->jump_slot < 4 &&
            state.jumpcfg == (1UL << (16 * demo->jump_slot)) &&
            state.jumps[2 * demo->jump_slot] == DASICS_BOUND_ALIGN_DOWN(jump_lo) &&
            state.jumps[2 * demo->jump_slot + 1] == DASICS_BOUND_ALIGN_UP(jump_hi);
    fdi_demo_check(demo, "SETUP", valid, 1);
    return valid && !demo->failed;
}

static int fdi_demo_call(struct fdi_demo *demo, int (*probe)(void))
{
    csr_write(CSR_USCRATCH, demo->recovery);
    int result = fdi_demo_invoke(demo->mode == DASICS_LINUX_EXEC_ON, probe);
    csr_write(CSR_USCRATCH, 0);
    return result;
}

static void fdi_demo_observe(const struct fdi_demo *demo)
{
    const volatile struct fdi_demo_observation *o = demo->observed;
    struct fdi_demo_state s = fdi_demo_read_state();
    int d = demo->data_slot, r = demo->observation_slot, j = demo->jump_slot;
    printf(FDI_DEMO_TAG " observation demo=%s dasics=%s function=%u "
        "initial_cfg=0x%lx initial_return_pc=0x%lx total_cfg=0x%lx "
        "data_slot=%d data_cfg=0x%lx data_lo=0x%lx data_hi=0x%lx "
        "observation_slot=%d observation_cfg=0x%lx observation_lo=0x%lx observation_hi=0x%lx "
        "jump_slot=%d jump_cfg=0x%lx jump_lo=0x%lx jump_hi=0x%lx "
        "source=0x%lx address=0x%lx site=0x%lx recovery=0x%lx target=0x%lx "
        "value=0x%lx read_value=0x%lx hits=0x%lx path=0x%lx link=0x%lx return_pc=0x%lx "
        "selected=0x%lx unchanged=0x%lx original_ra=0x%lx saved_ra=0x%lx "
        "sp_before=0x%lx sp_after=0x%lx service_count=0x%lx bound_preserved=0x%lx\n",
        demo->name, dasics_linux_exec_mode_name(demo->mode), demo->function,
        demo->original.libcfg, demo->original.return_pc, s.libcfg,
        d, d >= 0 ? (s.libcfg >> (4 * d)) & 15 : 0,
        d >= 0 ? s.bounds[2 * d] : 0, d >= 0 ? s.bounds[2 * d + 1] : 0,
        r, r >= 0 ? (s.libcfg >> (4 * r)) & 15 : 0,
        r >= 0 ? s.bounds[2 * r] : 0, r >= 0 ? s.bounds[2 * r + 1] : 0,
        j, s.jumpcfg, j >= 0 ? s.jumps[2 * j] : 0, j >= 0 ? s.jumps[2 * j + 1] : 0,
        demo->source, demo->address, demo->site, demo->recovery, demo->target,
        o->value, o->read_value, o->hits, o->path, o->link, s.return_pc,
        o->selected, o->unchanged, o->original_ra, o->saved_ra, o->sp_before,
        o->sp_after, o->service_count, o->bound_preserved);
}

static int fdi_demo_finish(struct fdi_demo *demo, int result)
{
    int protected = demo->mode == DASICS_LINUX_EXEC_ON;
    unsigned long faults = protected && demo->reason != 0;
    unsigned long after = dasics_n_extension_trap_count;
    unsigned long link = demo->observed ? demo->observed->link : 0;
    unsigned long expected_link = protected ? (uintptr_t)lib_call + 12 :
                                              (uintptr_t)fdi_demo_off_return;
    fdi_demo_check(demo, "CALL-RETURN", (unsigned long)(long)result, 0);
    fdi_demo_check(demo, "RETURN-PC",
        link == expected_link && csr_read(CSR_DRETURNPC) ==
            (protected ? expected_link : demo->original.return_pc), 1);
    fdi_demo_check(demo, "TRAP-COUNT", after - demo->before, faults);
    fdi_demo_check(demo, "FREASON", csr_read(CSR_DFREASON), faults ? demo->reason : 0);
    int context = after == demo->before + faults && !csr_read(CSR_USCRATCH);
    if (faults) {
        if (demo->before >= DASICS_N_EXTENSION_TRAP_RECORDS) {
            context = 0;
        } else {
            const volatile dasics_n_extension_trap_record_t *r =
                &dasics_n_extension_trap_records[demo->before];
            context &= r->ucause == DASICS_N_EXTENSION_UCHECK_CAUSE &&
                r->ustatus == 0x10 && csr_read(CSR_USTATUS) == 0x11 &&
                r->uepc == demo->site && r->utval == demo->address &&
                r->dfreason == demo->reason && r->uscratch == demo->recovery &&
                r->recovery == demo->recovery && r->utvec == (uintptr_t)dasics_ufault_entry &&
                r->gpr[0] == 0 && r->gpr[1] == demo->fault_ra &&
                r->gpr[5] == demo->source && r->gpr[6] == demo->fault_t1 &&
                r->gpr[7] == demo->fault_t2 && r->gpr[28] == demo->fault_t3 &&
                r->gpr[31] == expected_link;
        }
    }
    fdi_demo_check(demo, "U24-CONTEXT", context, 1);
    /* USTATUS and the ordered N trap stream belong to the shared destructor. */
    fdi_demo_check(demo, "STATE-RESTORE", fdi_demo_restore(&demo->original), 1);
    printf(FDI_DEMO_TAG " summary demo=%s dasics=%s function=%u total=%lu failed=%d result=%s\n",
           demo->name, dasics_linux_exec_mode_name(demo->mode), demo->function,
           demo->total, demo->failed, demo->failed ? "FAIL" : "PASS");
    return demo->failed ? 1 : 0;
}

#endif
