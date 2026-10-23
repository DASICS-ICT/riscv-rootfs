#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ucsr.h"
#include "udasics.h"

#define MEMORY_ISA_TAG "[DASICS-MEMORY-ISA]"

#if !DASICS_LINUX_DUAL_EXEC || !defined(DASICS_N_EXTENSION_PROFILE)
int main(void)
{
    fprintf(stderr, MEMORY_ISA_TAG " Linux N profile required result=FAIL\n");
    return 2;
}
#else
#define MEMORY_ISA_INITIAL 0x8877665544332211UL
#define MEMORY_ISA_STORE 0xf0e1d2c3b4a59687UL
#define MEMORY_ISA_POISON 0xfeedfacecafebeefUL
#define MEMORY_ISA_MATRIX_CHECKS 47UL
#define MEMORY_ISA_SLOTS_CHECKS 35UL
#define MEMORY_ISA_MATRIX_FAULTS 32UL

/* Every operand, including rejected endpoints, is valid allocated user RAM. */
static volatile unsigned char memory_isa_arena[320] __attribute__((aligned(64)));

struct memory_isa_observed {
    unsigned long value, operand;
};

struct memory_isa_probe {
    char *entry, *site, *recovery;
    int store;
};

/* Loads use the real destination x7. Stores have no architectural destination;
 * their continuation returns a control seed and the actual source x6. */
#define DEFINE_MEMORY_PROBE(name, instruction, operand) \
extern char name##_entry[], name##_site[], name##_recovery[]; \
asm(".option push\n.option norvc\n.option norelax\n" \
    ".section .ulibtext,\"ax\",@progbits\n.balign 8\n" \
    ".global " #name "_entry\n.type " #name "_entry,@function\n" \
    #name "_entry:\nmv t0,a1\nmv t1,a2\nli t2,0xfeedfacecafebeef\n" \
    ".global " #name "_site\n" #name "_site:\n" instruction "\n" \
    ".global " #name "_recovery\n" #name "_recovery:\nmv a0,t2\nmv a1," operand "\nret\n" \
    ".size " #name "_entry,.-" #name "_entry\n" \
    ".section .text,\"ax\",@progbits\n.option pop\n");

DEFINE_MEMORY_PROBE(memory_isa_u8_load, "lbu t2,0(t0)", "t0")
DEFINE_MEMORY_PROBE(memory_isa_u16_load, "lhu t2,0(t0)", "t0")
DEFINE_MEMORY_PROBE(memory_isa_u32_load, "lwu t2,0(t0)", "t0")
DEFINE_MEMORY_PROBE(memory_isa_u64_load, "ld t2,0(t0)", "t0")
DEFINE_MEMORY_PROBE(memory_isa_u8_store, "sb t1,0(t0)", "t1")
DEFINE_MEMORY_PROBE(memory_isa_u16_store, "sh t1,0(t0)", "t1")
DEFINE_MEMORY_PROBE(memory_isa_u32_store, "sw t1,0(t0)", "t1")
DEFINE_MEMORY_PROBE(memory_isa_u64_store, "sd t1,0(t0)", "t1")

#define MEMORY_PROBE_ROW(width, operation, is_store) \
    {memory_isa_u##width##_##operation##_entry, memory_isa_u##width##_##operation##_site, \
     memory_isa_u##width##_##operation##_recovery, is_store}
static const struct memory_isa_probe memory_isa_loads[] = {
    MEMORY_PROBE_ROW(8, load, 0), MEMORY_PROBE_ROW(16, load, 0),
    MEMORY_PROBE_ROW(32, load, 0), MEMORY_PROBE_ROW(64, load, 0)
};
static const struct memory_isa_probe memory_isa_stores[] = {
    MEMORY_PROBE_ROW(8, store, 1), MEMORY_PROBE_ROW(16, store, 1),
    MEMORY_PROBE_ROW(32, store, 1), MEMORY_PROBE_ROW(64, store, 1)
};

extern struct memory_isa_observed memory_isa_invoke(char *entry, volatile unsigned char *address,
                                                    unsigned long value);
extern char memory_isa_return[];
asm(".option push\n.option norvc\n.option norelax\n"
    ".section .text,\"ax\",@progbits\n.balign 8\n"
    ".global memory_isa_invoke\n.type memory_isa_invoke,@function\nmemory_isa_invoke:\n"
    "addi sp,sp,-16\nsd ra,8(sp)\n.word 0x000510fb\n"
    ".global memory_isa_return\nmemory_isa_return:\n"
    "csrw 0x040,zero\nld ra,8(sp)\naddi sp,sp,16\nret\n"
    ".size memory_isa_invoke,.-memory_isa_invoke\n.option pop\n");

#define MEMORY_BOUND_LIST(M) \
    M(0, 0x890, 0x891) M(1, 0x892, 0x893) M(2, 0x894, 0x895) M(3, 0x896, 0x897) \
    M(4, 0x898, 0x899) M(5, 0x89a, 0x89b) M(6, 0x89c, 0x89d) M(7, 0x89e, 0x89f) \
    M(8, 0x8a0, 0x8a1) M(9, 0x8a2, 0x8a3) M(10, 0x8a4, 0x8a5) M(11, 0x8a6, 0x8a7) \
    M(12, 0x8a8, 0x8a9) M(13, 0x8aa, 0x8ab) M(14, 0x8ac, 0x8ad) M(15, 0x8ae, 0x8af)

struct memory_isa_state {
    unsigned long cfg, lo[16], hi[16], return_pc, freason, uscratch;
};

static void read_state(struct memory_isa_state *state)
{
    state->cfg = csr_read(0x880);
#define READ_BOUND(index, low, high) state->lo[index] = csr_read(low); state->hi[index] = csr_read(high);
    MEMORY_BOUND_LIST(READ_BOUND)
#undef READ_BOUND
    state->return_pc = csr_read(0x8b1);
    state->freason = csr_read(0x8b3);
    state->uscratch = csr_read(CSR_USCRATCH);
}

static void write_bound(unsigned int slot, unsigned long lo, unsigned long hi)
{
    switch (slot) {
#define WRITE_BOUND(index, low, high) case index: csr_write(low, lo); csr_write(high, hi); break;
        MEMORY_BOUND_LIST(WRITE_BOUND)
#undef WRITE_BOUND
    }
}

static int configure(unsigned int slot, unsigned long nibble, unsigned long lo,
                      unsigned long hi, struct memory_isa_state *observed)
{
    csr_write(0x880, 0);
    for (unsigned int index = 0; index < 16; index++)
        write_bound(index, 0, 0);
    write_bound(slot, lo, hi);
    csr_write(0x8b3, 0);
    csr_write(0x880, nibble << (slot * 4));
    read_state(observed);
    int pass = observed->cfg == (nibble << (slot * 4));
    for (unsigned int index = 0; index < 16; index++) {
        pass = pass && observed->lo[index] == (index == slot ? lo & ~7UL : 0) &&
                       observed->hi[index] == (index == slot ? hi & ~7UL : 0);
    }
    return pass;
}

static unsigned long width_mask(unsigned int width)
{
    return width == 64 ? ~0UL : (1UL << width) - 1UL;
}

static void initialize_arena(unsigned int offset, unsigned int bytes)
{
    for (unsigned int index = 0; index < sizeof(memory_isa_arena); index++)
        memory_isa_arena[index] = 0x40 + index;
    for (unsigned int index = 0; index < bytes; index++)
        memory_isa_arena[offset + index] = MEMORY_ISA_INITIAL >> (index * 8);
}

static unsigned long read_operand(unsigned int offset, unsigned int bytes)
{
    unsigned long value = 0;
    for (unsigned int index = 0; index < bytes; index++)
        value |= (unsigned long)memory_isa_arena[offset + index] << (index * 8);
    return value;
}

static int arena_matches(unsigned int offset, unsigned int bytes, unsigned long value)
{
    int pass = 1;
    for (unsigned int index = 0; index < sizeof(memory_isa_arena); index++) {
        unsigned char expected = index >= offset && index < offset + bytes ?
            value >> ((index - offset) * 8) : 0x40 + index;
        pass = pass && memory_isa_arena[index] == expected;
    }
    return pass;
}

static int run_case(const struct memory_isa_probe *probe, unsigned int width, const char *name,
                     unsigned int slot, unsigned long nibble, unsigned int offset,
                     unsigned int lo_offset, unsigned int hi_offset, unsigned long *total)
{
    unsigned int bytes = width / 8;
    unsigned long address = (unsigned long)&memory_isa_arena[offset];
    unsigned long lo_requested = (unsigned long)&memory_isa_arena[lo_offset];
    unsigned long hi_requested = (unsigned long)&memory_isa_arena[hi_offset];
    unsigned long lo = lo_requested & ~7UL, hi = hi_requested & ~7UL;
    unsigned long last = address + bytes - 1;
    /* LibCfg bit 2 is retained in CSR readback but does not participate in the
     * current checker permission match. Each byte range needs one entry. */
    int allow = (nibble & 8) && (nibble & (probe->store ? 1 : 2)) &&
        address >= lo && last < hi && last >= address;
    unsigned long reason_expected = allow ? 0 : probe->store ? 3 : 2;
    unsigned long value_expected = !probe->store && allow ? MEMORY_ISA_INITIAL & width_mask(width) : MEMORY_ISA_POISON;
    unsigned long operand_expected = probe->store ? MEMORY_ISA_STORE : address;
    unsigned long data_expected = (probe->store && allow ? MEMORY_ISA_STORE : MEMORY_ISA_INITIAL) & width_mask(width);
    unsigned long before = dasics_n_extension_trap_count;
    struct memory_isa_state configured;
    initialize_arena(offset, bytes);
    int config_ok = configure(slot, nibble, lo_requested, hi_requested, &configured);
    csr_write(CSR_USCRATCH, (unsigned long)probe->recovery);
    struct memory_isa_observed observed = memory_isa_invoke(probe->entry, &memory_isa_arena[offset], MEMORY_ISA_STORE);
    unsigned long after = dasics_n_extension_trap_count;
    unsigned long reason = csr_read(0x8b3), return_pc = csr_read(0x8b1), scratch = csr_read(CSR_USCRATCH);
    unsigned long data = read_operand(offset, bytes);
    unsigned long cause = 0, status = 0, pc = 0, tval = 0, recovery = 0;
    unsigned long vector = 0, trap_address = 0, trap_operand = 0, trap_destination = 0;
    unsigned long trap_scratch = 0, trap_ra = 0;
    int trap_ok = after == before + !allow;
    if (!allow) {
        if (before >= DASICS_N_EXTENSION_TRAP_RECORDS || after != before + 1) {
            trap_ok = 0;
        } else {
            const volatile dasics_n_extension_trap_record_t *record = &dasics_n_extension_trap_records[before];
            cause = record->ucause; status = record->ustatus; pc = record->uepc;
            tval = record->utval; recovery = record->recovery; vector = record->utvec;
            trap_address = record->gpr[5]; trap_operand = record->gpr[6];
            trap_destination = probe->store ? 0 : record->gpr[7];
            trap_scratch = record->uscratch; trap_ra = record->gpr[1];
            trap_ok = trap_ok && cause == 24 && status == 0x10 && pc == (unsigned long)probe->site &&
                tval == address && record->dfreason == reason_expected &&
                recovery == (unsigned long)probe->recovery && trap_scratch == recovery &&
                vector == (unsigned long)dasics_ufault_entry && trap_address == address &&
                trap_operand == MEMORY_ISA_STORE && (!probe->store ? trap_destination == MEMORY_ISA_POISON : 1) &&
                trap_ra == (unsigned long)memory_isa_return;
        }
    }
    int arena_ok = arena_matches(offset, bytes, data_expected);
    unsigned long post_status = csr_read(CSR_USTATUS);
    unsigned long status_expected = after ? 0x11 : 0x1;
    int pass = config_ok && trap_ok && reason == reason_expected && observed.value == value_expected &&
        observed.operand == operand_expected && data == data_expected && arena_ok &&
        return_pc == (unsigned long)memory_isa_return && scratch == 0 && post_status == status_expected;
    (*total)++;
    printf(MEMORY_ISA_TAG " case=MEM%u-%s-%s value=0x%lx expect=0x1 slot=%u nibble=0x%lx "
        "cfg=0x%lx cfg_expected=0x%lx lo_requested=0x%lx hi_requested=0x%lx "
        "lo=0x%lx lo_expected=0x%lx hi=0x%lx hi_expected=0x%lx addr=0x%lx last=0x%lx bytes=%u "
        "destination=%s result_gpr=0x%lx result_gpr_expected=0x%lx operand=0x%lx operand_expected=0x%lx "
        "data=0x%lx data_expected=0x%lx arena_ok=%d arena_expected=1 config_ok=%d config_expected=1 "
        "freason=0x%lx freason_expected=0x%lx traps=%lu->%lu trap_delta_expected=%d "
        "cause=0x%lx cause_expected=0x%lx ustatus=0x%lx ustatus_expected=0x%lx "
        "uepc=0x%lx uepc_expected=0x%lx utval=0x%lx utval_expected=0x%lx "
        "recovery=0x%lx recovery_expected=0x%lx utvec=0x%lx utvec_expected=0x%lx "
        "trap_address=0x%lx trap_address_expected=0x%lx trap_operand=0x%lx trap_operand_expected=0x%lx "
        "trap_destination=0x%lx trap_destination_expected=0x%lx trap_uscratch=0x%lx trap_uscratch_expected=0x%lx "
        "trap_ra=0x%lx trap_ra_expected=0x%lx returnpc=0x%lx returnpc_expected=0x%lx "
        "uscratch=0x%lx uscratch_expected=0x0 post_ustatus=0x%lx post_ustatus_expected=0x%lx result=%s\n",
        width, probe->store ? "STORE" : "LOAD", name, (unsigned long)pass, slot, nibble,
        configured.cfg, nibble << (slot * 4), lo_requested, hi_requested, configured.lo[slot], lo,
        configured.hi[slot], hi, address, last, bytes, probe->store ? "NONE" : "x7",
        observed.value, value_expected, observed.operand, operand_expected, data, data_expected,
        arena_ok, config_ok, reason, reason_expected, before, after, !allow,
        cause, allow ? 0UL : 24UL, status, allow ? 0UL : 0x10UL,
        pc, allow ? 0UL : (unsigned long)probe->site, tval, allow ? 0UL : address,
        recovery, allow ? 0UL : (unsigned long)probe->recovery,
        vector, allow ? 0UL : (unsigned long)dasics_ufault_entry,
        trap_address, allow ? 0UL : address, trap_operand, allow ? 0UL : MEMORY_ISA_STORE,
        trap_destination, !allow && !probe->store ? MEMORY_ISA_POISON : 0UL,
        trap_scratch, allow ? 0UL : (unsigned long)probe->recovery,
        trap_ra, allow ? 0UL : (unsigned long)memory_isa_return,
        return_pc, (unsigned long)memory_isa_return, scratch, post_status, status_expected, pass ? "PASS" : "FAIL");
    return !pass;
}

static int restore_state(const struct memory_isa_state *saved, const unsigned char *arena,
                          unsigned long *total)
{
    struct memory_isa_state actual;
    csr_write(0x880, 0);
    for (unsigned int slot = 0; slot < 16; slot++)
        write_bound(slot, saved->lo[slot], saved->hi[slot]);
    csr_write(0x8b1, saved->return_pc);
    csr_write(0x8b3, saved->freason);
    csr_write(CSR_USCRATCH, saved->uscratch);
    csr_write(0x880, saved->cfg);
    for (unsigned int index = 0; index < sizeof(memory_isa_arena); index++)
        memory_isa_arena[index] = arena[index];
    read_state(&actual);
    int bounds_ok = !memcmp(actual.lo, saved->lo, sizeof(actual.lo)) &&
                    !memcmp(actual.hi, saved->hi, sizeof(actual.hi));
    for (unsigned int slot = 0; slot < 16; slot++) {
        printf("MEMORY_ISA_RESTORE_BOUND slot=%u lo=0x%lx lo_expected=0x%lx "
            "hi=0x%lx hi_expected=0x%lx result=%s\n", slot, actual.lo[slot], saved->lo[slot],
            actual.hi[slot], saved->hi[slot],
            actual.lo[slot] == saved->lo[slot] && actual.hi[slot] == saved->hi[slot] ? "PASS" : "FAIL");
    }
    int data_ok = 1;
    for (unsigned int index = 0; index < sizeof(memory_isa_arena); index++)
        data_ok = data_ok && memory_isa_arena[index] == arena[index];
    int pass = !memcmp(&actual, saved, sizeof(actual)) && data_ok;
    (*total)++;
    printf(MEMORY_ISA_TAG " case=STATE-RESTORE value=0x%lx expect=0x1 cfg=0x%lx cfg_expected=0x%lx "
        "returnpc=0x%lx returnpc_expected=0x%lx freason=0x%lx freason_expected=0x%lx "
        "uscratch=0x%lx uscratch_expected=0x%lx bounds_ok=%d bounds_expected=1 arena_ok=%d arena_expected=1 result=%s\n",
        (unsigned long)pass, actual.cfg, saved->cfg, actual.return_pc, saved->return_pc,
        actual.freason, saved->freason, actual.uscratch, saved->uscratch, bounds_ok, data_ok, pass ? "PASS" : "FAIL");
    return !pass;
}

int main(int argc, char *argv[])
{
    unsigned int width = 64, width_index = 3;
    int slots = 0;
    for (int index = 1; index < argc; index++) {
        if (!strcmp(argv[index], "--width=8")) { width = 8; width_index = 0; }
        else if (!strcmp(argv[index], "--width=16")) { width = 16; width_index = 1; }
        else if (!strcmp(argv[index], "--width=32")) { width = 32; width_index = 2; }
        else if (!strcmp(argv[index], "--width=64")) { width = 64; width_index = 3; }
        else if (!strcmp(argv[index], "--scenario=matrix")) slots = 0;
        else if (!strcmp(argv[index], "--scenario=slots")) slots = 1;
        else { fprintf(stderr, MEMORY_ISA_TAG " argument-contract result=FAIL\n"); return 2; }
    }
    if (slots && width != 64) {
        fprintf(stderr, MEMORY_ISA_TAG " slots-requires-width64 result=FAIL\n");
        return 2;
    }
    unsigned long expected_checks = slots ? MEMORY_ISA_SLOTS_CHECKS : MEMORY_ISA_MATRIX_CHECKS;
    unsigned long expected_faults = slots ? 0 : MEMORY_ISA_MATRIX_FAULTS;
    unsigned long total = 0, before = dasics_n_extension_trap_count;
    unsigned long initial_status = csr_read(CSR_USTATUS);
    unsigned long cfg = dasics_linux_query_umaincfg();
    int failures = 0;
    const char *scenario = slots ? "slots" : "matrix";
    printf(MEMORY_ISA_TAG " begin version=1 scenario=%s width=%u expected_total=%lu expected_u24=%lu\n",
           scenario, width, expected_checks, expected_faults);
    int profile = cfg == 2 && before == 0 && initial_status == 1 &&
        DASICS_N_EXTENSION_TRAP_RECORDS >= expected_faults;
    total++;
    failures += !profile;
    printf(MEMORY_ISA_TAG " case=PROFILE value=0x%lx expect=0x1 umaincfg=0x%lx umaincfg_expected=0x2 "
           "traps=%lu traps_expected=0 ustatus=0x%lx ustatus_expected=0x1 result=%s\n",
           (unsigned long)profile, cfg, before, initial_status, profile ? "PASS" : "FAIL");
    if (profile) {
        struct memory_isa_state saved;
        unsigned char saved_data[sizeof(memory_isa_arena)];
        read_state(&saved);
        for (unsigned int index = 0; index < sizeof(memory_isa_arena); index++)
            saved_data[index] = memory_isa_arena[index];
        if (slots) {
            for (unsigned int slot = 0; slot < 16; slot++) {
                char name[32];
                snprintf(name, sizeof(name), "SLOT%u-RW-BIT2-%u", slot, slot & 1);
                unsigned int offset = 16 + slot * 16;
                unsigned long nibble = slot & 1 ? 0xf : 0xb;
                failures += run_case(&memory_isa_loads[width_index], width, name, slot, nibble,
                                     offset, offset + 3, offset + 15, &total);
                failures += run_case(&memory_isa_stores[width_index], width, name, slot, nibble,
                                     offset, offset + 3, offset + 15, &total);
            }
        } else {
            for (unsigned long nibble = 0; nibble < 16; nibble++) {
                char name[24];
                snprintf(name, sizeof(name), "CFG%lx", nibble);
                failures += run_case(&memory_isa_loads[width_index], width, name, 0, nibble, 24, 16, 48, &total);
                failures += run_case(&memory_isa_stores[width_index], width, name, 0, nibble, 24, 16, 48, &total);
            }
            const unsigned int offsets[] = {16 - width / 8, 16, 32 - width / 8, 32};
            const char *names[] = {"LO-MINUS-WIDTH", "LO", "HI-MINUS-WIDTH", "HI"};
            for (unsigned int index = 0; index < 4; index++) {
                failures += run_case(&memory_isa_loads[width_index], width, names[index], 0, 0xb,
                                     offsets[index], 16, 32, &total);
                failures += run_case(&memory_isa_stores[width_index], width, names[index], 0, 0xb,
                                     offsets[index], 16, 32, &total);
            }
            failures += run_case(&memory_isa_loads[width_index], width, "QUANTIZED-EMPTY", 0, 0xb, 24, 24, 28, &total);
            failures += run_case(&memory_isa_stores[width_index], width, "QUANTIZED-EMPTY", 0, 0xb, 24, 24, 28, &total);
            failures += run_case(&memory_isa_loads[width_index], width, "QUANTIZED-REVERSE", 0, 0xb, 24, 35, 19, &total);
            failures += run_case(&memory_isa_stores[width_index], width, "QUANTIZED-REVERSE", 0, 0xb, 24, 35, 19, &total);
        }
        failures += restore_state(&saved, saved_data, &total);
        unsigned long after = dasics_n_extension_trap_count, status = csr_read(CSR_USTATUS);
        unsigned long status_expected = expected_faults ? 0x11 : initial_status;
        int runtime_ok = after == before + expected_faults && status == status_expected;
        total++;
        failures += !runtime_ok;
        printf(MEMORY_ISA_TAG " case=N-RUNTIME-GUARD value=0x%lx expect=0x1 u24=%lu u24_expected=%lu "
            "ustatus=0x%lx ustatus_expected=0x%lx result=%s\n", (unsigned long)runtime_ok,
            after, before + expected_faults, status, status_expected, runtime_ok ? "PASS" : "FAIL");
    }
    if (total != expected_checks)
        failures++;
    printf(MEMORY_ISA_TAG " scope=ordinary-aligned-load-store grain_bytes=8 libcfg_bit2=NOT_USED_FOR_PERMISSION "
           "partial_span=NOT_OBSERVED cross_slot_join=NOT_OBSERVED uint64_wrap=NOT_OBSERVED "
           "normal_misaligned_exception=HIGHER_PRIORITY_THAN_FDI\n");
    printf(MEMORY_ISA_TAG " summary version=1 scenario=%s width=%u total=%lu failed=%d result=%s\n",
           scenario, width, total, failures, failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
#endif
