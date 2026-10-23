#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "udasics.h"

#define DYNAMIC_TAG "[DASICS-DYNAMIC-UNIFIED]"

#if DASICS_LINUX_DUAL_EXEC && defined(DASICS_N_EXTENSION_PROFILE)
#define LOAD_SEED 0x44594e424c464c54UL
#define STORE_MARKER 0x44594e4253464c54UL
#define BUFFER_BYTES 104UL
#define SLOT_COUNT 16
#define BOUND_LIST(M) \
    M(0, 0x890, 0x891) M(1, 0x892, 0x893) M(2, 0x894, 0x895) M(3, 0x896, 0x897) \
    M(4, 0x898, 0x899) M(5, 0x89a, 0x89b) M(6, 0x89c, 0x89d) M(7, 0x89e, 0x89f) \
    M(8, 0x8a0, 0x8a1) M(9, 0x8a2, 0x8a3) M(10, 0x8a4, 0x8a5) M(11, 0x8a6, 0x8a7) \
    M(12, 0x8a8, 0x8a9) M(13, 0x8aa, 0x8ab) M(14, 0x8ac, 0x8ad) M(15, 0x8ae, 0x8af)

struct dynamic_data {
    unsigned char read_data[8], write_data[8], slots[SLOT_COUNT][8], target[8];
    unsigned char library_buffer[BUFFER_BYTES];
};
static volatile struct dynamic_data dynamic_data ATTR_ULIB_DATA = {
    .read_data = {0x6d}, .write_data = {0x29}, .target = {0x73},
    .library_buffer = "dynamic bound data",
};

struct bound_table {
    unsigned long cfg, lo[SLOT_COUNT], hi[SLOT_COUNT];
};
struct owned_state {
    struct bound_table table;
    unsigned long maincall, return_pc, active_return, freason, scratch, vector, helper;
};
struct direct_result { unsigned long value, link; };
extern uint64_t umaincall_helper;
extern struct direct_result dynamic_direct_invoke(char *entry, volatile unsigned char *address,
                                                 unsigned long value);
extern char dynamic_unified_load_entry[], dynamic_unified_store_entry[], dynamic_unified_probe_return[];
extern char dynamic_unified_load_site[], dynamic_unified_load_recovery[];
extern char dynamic_unified_store_site[], dynamic_unified_store_recovery[];
extern char dynamic_unified_library_site[], dynamic_unified_library_recovery[];

/* The direct probes use no stack memory, so all sixteen data slots can be
 * filled with test regions. The wrapper's stack accesses remain trusted. */
asm (
    ".option push\n.option norvc\n.option norelax\n"
    ".section .text,\"ax\",@progbits\n.balign 8\n"
    ".global dynamic_direct_invoke\n.type dynamic_direct_invoke,@function\n"
    "dynamic_direct_invoke:\naddi sp,sp,-16\nsd ra,8(sp)\n.word 0x000510fb\n"
    ".global dynamic_unified_probe_return\ndynamic_unified_probe_return:\n"
    "csrw uscratch,zero\nmv a1,ra\nld ra,8(sp)\naddi sp,sp,16\nret\n"
    ".size dynamic_direct_invoke,.-dynamic_direct_invoke\n"
    ".section .ulibtext,\"ax\",@progbits\n.balign 8\n"
    ".global dynamic_unified_load_entry\ndynamic_unified_load_entry:\n"
    "mv t0,a1\nli t2,0x44594e424c464c54\nmv a0,t2\n"
    ".global dynamic_unified_load_site\ndynamic_unified_load_site:\nlbu t2,0(t0)\n"
    ".global dynamic_unified_load_recovery\ndynamic_unified_load_recovery:\nmv a0,t2\nret\n"
    ".balign 8\n.global dynamic_unified_store_entry\ndynamic_unified_store_entry:\n"
    "mv t0,a1\nmv t1,a2\nli a0,0x44594e4253464c54\n"
    ".global dynamic_unified_store_site\ndynamic_unified_store_site:\nsb t1,0(t0)\n"
    ".global dynamic_unified_store_recovery\ndynamic_unified_store_recovery:\nret\n"
    ".section .text,\"ax\",@progbits\n.option pop\n"
);

#pragma GCC push_options
#pragma GCC optimize("O0")
static int __attribute__((noinline, used)) ATTR_ULIB_TEXT dynamic_library_store(void)
{
    /* Keep the library test's ordinary C entry and byte store after release. */
    asm volatile (
        ".option push\n.option norvc\n.option norelax\nmv t0,%0\nli t1,88\n"
        ".global dynamic_unified_library_site\ndynamic_unified_library_site:\nsb t1,0(t0)\n"
        ".global dynamic_unified_library_recovery\ndynamic_unified_library_recovery:\n.option pop\n"
        : : "r"(&dynamic_data.library_buffer[0]) : "t0", "t1", "memory");
    return 0;
}
#pragma GCC pop_options

static void read_table(struct bound_table *table)
{
    table->cfg = csr_read(CSR_DLCFG0);
#define READ_BOUND(index, lo_csr, hi_csr) \
    table->lo[index] = csr_read(lo_csr); table->hi[index] = csr_read(hi_csr);
    BOUND_LIST(READ_BOUND)
#undef READ_BOUND
}

static void write_bound(unsigned int index, unsigned long lo, unsigned long hi)
{
    switch (index) {
#define WRITE_BOUND(index, lo_csr, hi_csr) \
    case index: csr_write(lo_csr, lo); csr_write(hi_csr, hi); break;
    BOUND_LIST(WRITE_BOUND)
#undef WRITE_BOUND
    }
}

static int table_equal(const struct bound_table *left, const struct bound_table *right)
{
    return left->cfg == right->cfg && !memcmp(left->lo, right->lo, sizeof(left->lo)) &&
           !memcmp(left->hi, right->hi, sizeof(left->hi));
}

static void snapshot(struct owned_state *state)
{
    read_table(&state->table);
    state->maincall = csr_read(CSR_DMAINCALL);
    state->return_pc = csr_read(CSR_DRETURNPC);
    state->active_return = csr_read(CSR_DFZRETURN);
    state->freason = csr_read(CSR_DFREASON);
    state->scratch = csr_read(CSR_USCRATCH);
    state->vector = csr_read(CSR_UTVEC);
    state->helper = umaincall_helper;
}

static int data_equal(const struct dynamic_data *expected)
{
    const volatile unsigned char *actual = (const volatile unsigned char *)&dynamic_data;
    const unsigned char *wanted = (const unsigned char *)expected;
    for (unsigned int i = 0; i < sizeof(dynamic_data); i++)
        if (actual[i] != wanted[i])
            return 0;
    return 1;
}

static void copy_data(const struct dynamic_data *source)
{
    volatile unsigned char *actual = (volatile unsigned char *)&dynamic_data;
    const unsigned char *bytes = (const unsigned char *)source;
    for (unsigned int i = 0; i < sizeof(dynamic_data); i++)
        actual[i] = bytes[i];
}

static int record_value(const char *name, unsigned long actual, unsigned long expected,
                         unsigned long *total)
{
    int pass = actual == expected;
    (*total)++;
    printf(DYNAMIC_TAG " case=%s actual=0x%lx expect=0x%lx result=%s\n",
           name, actual, expected, pass ? "PASS" : "FAIL");
    return !pass;
}

static int record_table(const char *name, const struct bound_table *expected, unsigned int slot,
    unsigned long actual_value, unsigned long expected_value, unsigned long reason,
    unsigned long traps, unsigned long *total)
{
    struct bound_table actual;
    read_table(&actual);
    int all_slots_ok = table_equal(&actual, expected);
    int pass = actual_value == expected_value && all_slots_ok &&
        csr_read(CSR_DFREASON) == reason && dasics_n_extension_trap_count == traps;
    (*total)++;
    printf(DYNAMIC_TAG " case=%s actual=0x%lx expect=0x%lx cfg=0x%lx expect_cfg=0x%lx "
           "slot=%u lo=0x%lx expect_lo=0x%lx hi=0x%lx expect_hi=0x%lx "
           "all_slots_ok=%d expect_all_slots_ok=1 freason=0x%lx expect_freason=0x%lx "
           "traps=%lu expect_traps=%lu result=%s\n", name, actual_value, expected_value,
           actual.cfg, expected->cfg, slot, actual.lo[slot], expected->lo[slot],
           actual.hi[slot], expected->hi[slot], all_slots_ok,
           csr_read(CSR_DFREASON), reason, (unsigned long)dasics_n_extension_trap_count,
           traps, pass ? "PASS" : "FAIL");
    return !pass;
}

static int record_context(const char *name, unsigned long index, unsigned long after,
    char *site, char *recovery, unsigned long address, int store, unsigned long operand,
    unsigned long return_pc, unsigned long *total)
{
    unsigned long cause = 0, status = 0, pc = 0, tval = 0, reason = 0, resume = 0;
    unsigned long scratch = 0, vector = 0, source = 0, gpr = 0, ra = 0;
    unsigned long expected_reason = store ? EXC_DASICS_STORE_FAULT : EXC_DASICS_LOAD_FAULT;
    unsigned long expected_gpr = store ? operand : LOAD_SEED;
    int available = index < DASICS_N_EXTENSION_TRAP_RECORDS && after == index + 1;
    if (available) {
        const volatile dasics_n_extension_trap_record_t *record = &dasics_n_extension_trap_records[index];
        cause = record->ucause; status = record->ustatus; pc = record->uepc; tval = record->utval;
        reason = record->dfreason; resume = record->recovery; scratch = record->uscratch;
        vector = record->utvec; source = record->gpr[5]; gpr = record->gpr[store ? 6 : 7];
        ra = record->gpr[1];
    }
    unsigned long post_status = csr_read(CSR_USTATUS);
    int pass = available && cause == 24 && status == 0x10 && pc == (uintptr_t)site &&
        tval == address && reason == expected_reason && resume == (uintptr_t)recovery &&
        scratch == (uintptr_t)recovery && vector == (uintptr_t)dasics_ufault_entry &&
        source == address && gpr == expected_gpr && ra == return_pc && post_status == 0x11;
    (*total)++;
    printf(DYNAMIC_TAG " case=%s-U24 actual=0x%lx expect=0x18 sequence=%lu "
           "ustatus=0x%lx expect_ustatus=0x10 uepc=0x%lx expect_uepc=0x%lx "
           "utval=0x%lx expect_utval=0x%lx freason=0x%lx expect_freason=0x%lx "
           "recovery=0x%lx expect_recovery=0x%lx uscratch=0x%lx expect_uscratch=0x%lx "
           "utvec=0x%lx expect_utvec=0x%lx source=0x%lx expect_source=0x%lx "
           "gpr=0x%lx expect_gpr=0x%lx ra=0x%lx expect_ra=0x%lx "
           "post_ustatus=0x%lx expect_post_ustatus=0x11 result=%s\n",
           name, cause, index + 1, status, pc, (uintptr_t)site, tval, address, reason, expected_reason,
           resume, (uintptr_t)recovery, scratch, (uintptr_t)recovery, vector, (uintptr_t)dasics_ufault_entry,
           source, address, gpr, expected_gpr, ra, return_pc, post_status, pass ? "PASS" : "FAIL");
    return !pass;
}

static int run_access(const char *name, int library, int protected, int store, int allow,
    volatile unsigned char *address, unsigned long operand, struct dynamic_data *expected_data,
    const struct bound_table *expected_table, unsigned long *total)
{
    char *site = library ? dynamic_unified_library_site :
                 store ? dynamic_unified_store_site : dynamic_unified_load_site;
    char *recovery = library ? dynamic_unified_library_recovery :
                     store ? dynamic_unified_store_recovery : dynamic_unified_load_recovery;
    unsigned long offset = (uintptr_t)address - (uintptr_t)&dynamic_data;
    unsigned char *expected_byte = (unsigned char *)expected_data + offset;
    unsigned long before = dasics_n_extension_trap_count, old_pc = csr_read(CSR_DRETURNPC);
    unsigned long result, link = 0;
    int denied = !allow;
    if (store && allow)
        *expected_byte = operand;
    csr_write(CSR_DFREASON, 0);
    csr_write(CSR_USCRATCH, (uintptr_t)recovery);
    if (library) {
        result = protected ? lib_call(dynamic_library_store) : dynamic_library_store();
        csr_write(CSR_USCRATCH, 0);
    } else {
        struct direct_result observed = dynamic_direct_invoke(
            store ? dynamic_unified_store_entry : dynamic_unified_load_entry, address, operand);
        result = observed.value;
        link = observed.link;
    }
    unsigned long after = dasics_n_extension_trap_count;
    unsigned long reason = csr_read(CSR_DFREASON), return_pc = csr_read(CSR_DRETURNPC);
    unsigned long expected_pc = library ? old_pc : (uintptr_t)dynamic_unified_probe_return;
    if (library && protected && before < DASICS_N_EXTENSION_TRAP_RECORDS && after == before + 1)
        expected_pc = dasics_n_extension_trap_records[before].gpr[1];
    unsigned long expected_reason = denied ? (store ? EXC_DASICS_STORE_FAULT : EXC_DASICS_LOAD_FAULT) : 0;
    struct bound_table actual_table;
    read_table(&actual_table);
    int control = after == before + denied && reason == expected_reason &&
        csr_read(CSR_USCRATCH) == 0 && return_pc == expected_pc &&
        (library ? result == 0 : link == expected_pc && (!store || result == STORE_MARKER)) &&
        (!protected || expected_pc != 0) && table_equal(&actual_table, expected_table);
    char case_name[96];
    unsigned long value = store ? *address : result;
    unsigned long expected_value = store ? *expected_byte : allow ? *expected_byte : LOAD_SEED;
    int failures = 0;
    snprintf(case_name, sizeof(case_name), "%s-VALUE", name);
    failures += record_value(case_name, value, expected_value, total);
    snprintf(case_name, sizeof(case_name), "%s-DATA", name);
    failures += record_value(case_name, data_equal(expected_data), 1, total);
    (*total)++;
    printf(DYNAMIC_TAG " case=%s-CONTROL actual=0x%lx expect=0x%lx "
           "freason=0x%lx expect_freason=0x%lx traps=%lu->%lu expect_delta=%d "
           "gpr=0x%lx expect_gpr=0x%lx result=%s\n", name, return_pc, expected_pc,
           reason, expected_reason, before, after, denied, result,
           library ? 0UL : store ? STORE_MARKER : expected_value, control ? "PASS" : "FAIL");
    failures += !control;
    if (denied)
        failures += record_context(name, before, after, site, recovery, (uintptr_t)address,
                                   store, operand, return_pc, total);
    return failures;
}

static void set_direct_slot(struct bound_table *table, unsigned int slot,
                             volatile unsigned char *address, unsigned long access)
{
    write_bound(slot, (uintptr_t)address, (uintptr_t)address + 8);
    table->lo[slot] = (uintptr_t)address;
    table->hi[slot] = (uintptr_t)address + 8;
    /* Replace the complete nibble: a revoked R grant must not survive W reconfiguration. */
    table->cfg = (table->cfg & ~(0xfUL << (4 * slot))) | ((DASICS_LIBCFG_V | access) << (4 * slot));
    csr_write(CSR_DLCFG0, table->cfg);
    csr_write(CSR_DFREASON, 0);
}

static int run_direct(struct dynamic_data *expected_data, const struct bound_table *original,
                       unsigned long *total)
{
    struct bound_table table = *original;
    unsigned long start = dasics_n_extension_trap_count;
    int failures = 0;
    table.cfg = 0;
    csr_write(CSR_DLCFG0, 0);
    expected_data->read_data[0] = 0x6d;
    expected_data->write_data[0] = 0x29;
    expected_data->target[0] = 0x73;
    for (unsigned int i = 0; i < SLOT_COUNT; i++) {
        for (unsigned int j = 0; j < 8; j++)
            expected_data->slots[i][j] = 0x40 + i + j;
    }
    expected_data->slots[15][0] = 0x8f;
    copy_data(expected_data);

    set_direct_slot(&table, 0, dynamic_data.read_data, DASICS_LIBCFG_R);
    failures += record_table("DIRECT-GRANT-R", &table, 0, csr_read(CSR_DLCFG0), table.cfg, 0, start, total);
    failures += run_access("DIRECT-READ", 0, 1, 0, 1, dynamic_data.read_data, 0, expected_data, &table, total);
    table.cfg &= ~DASICS_LIBCFG_V;
    csr_write(CSR_DLCFG0, table.cfg);
    failures += record_table("DIRECT-REVOKE-R", &table, 0, csr_read(CSR_DLCFG0), table.cfg, 0, start, total);
    failures += run_access("DIRECT-READ-REVOKED", 0, 1, 0, 0, dynamic_data.read_data, 0, expected_data, &table, total);

    set_direct_slot(&table, 0, dynamic_data.write_data, DASICS_LIBCFG_W);
    failures += record_table("DIRECT-RECONFIG-W", &table, 0, csr_read(CSR_DLCFG0), table.cfg, 0, start + 1, total);
    failures += run_access("DIRECT-WRITE", 0, 1, 1, 1, dynamic_data.write_data, 0x71, expected_data, &table, total);
    table.cfg &= ~DASICS_LIBCFG_V;
    csr_write(CSR_DLCFG0, table.cfg);
    failures += record_table("DIRECT-REVOKE-W", &table, 0, csr_read(CSR_DLCFG0), table.cfg, 0, start + 1, total);
    failures += run_access("DIRECT-WRITE-REVOKED", 0, 1, 1, 0, dynamic_data.write_data, 0x72, expected_data, &table, total);

    csr_write(CSR_DLCFG0, 0);
    table.cfg = 0;
    for (unsigned int i = 0; i < SLOT_COUNT; i++)
        set_direct_slot(&table, i, dynamic_data.slots[i], DASICS_LIBCFG_R);
    failures += record_table("DIRECT-FILL-16-R", &table, 15, csr_read(CSR_DLCFG0), 0xaaaaaaaaaaaaaaaaUL, 0, start + 2, total);
    set_direct_slot(&table, 7, dynamic_data.target, DASICS_LIBCFG_R);
    failures += record_table("DIRECT-REPLACE-SLOT7", &table, 7, csr_read(CSR_DLCFG0), 0xaaaaaaaaaaaaaaaaUL, 0, start + 2, total);
    failures += run_access("DIRECT-REPLACEMENT-TARGET", 0, 1, 0, 1, dynamic_data.target, 0, expected_data, &table, total);
    failures += run_access("DIRECT-OLD-SLOT7", 0, 1, 0, 0, dynamic_data.slots[7], 0, expected_data, &table, total);
    failures += run_access("DIRECT-SLOT15-PRESERVED", 0, 1, 0, 1, dynamic_data.slots[15], 0, expected_data, &table, total);
    failures += record_value("TOTAL-TRAP-DELTA", dasics_n_extension_trap_count - start, 3, total);
    return failures;
}

static int run_library(int protected, struct dynamic_data *expected_data,
                        const struct bound_table *original, unsigned long *total)
{
    struct bound_table table = *original;
    unsigned long start = dasics_n_extension_trap_count;
    int expected_slot = -1, failures = 0;
    for (unsigned int i = 0; i < SLOT_COUNT; i++) {
        if (!(table.cfg & (DASICS_LIBCFG_V << (4 * i)))) {
            expected_slot = i;
            break;
        }
    }
    int slot = dasics_libcfg_alloc(DASICS_LIBCFG_R | DASICS_LIBCFG_W,
        DASICS_BOUND_ALIGN_DOWN(dynamic_data.library_buffer),
        DASICS_BOUND_ALIGN_UP(dynamic_data.library_buffer + 100));
    if (expected_slot < 0 || slot != expected_slot) {
        failures += record_value("LIBRARY-ALLOC-SLOT", (unsigned long)(long)slot,
                                 (unsigned long)(long)expected_slot, total);
        return failures + 1;
    }
    table.lo[slot] = (uintptr_t)dynamic_data.library_buffer;
    table.hi[slot] = (uintptr_t)dynamic_data.library_buffer + BUFFER_BYTES;
    table.cfg = (table.cfg & ~(0xfUL << (4 * slot))) | (0xbUL << (4 * slot));
    csr_write(CSR_DFREASON, 0);
    failures += record_table("LIBRARY-ALLOC", &table, slot, slot, expected_slot, 0, start, total);
    failures += record_value("LIBRARY-GET-ALLOCATED-CFG", dasics_libcfg_get(slot),
                              DASICS_LIBCFG_V | DASICS_LIBCFG_R | DASICS_LIBCFG_W, total);
    int ret = dasics_libcfg_free(slot);
    table.cfg &= ~(DASICS_LIBCFG_V << (4 * slot));
    failures += record_table("LIBRARY-FREE", &table, slot, (unsigned long)(long)ret, 0, 0, start, total);
    failures += record_value("LIBRARY-GET-RELEASED-CFG", dasics_libcfg_get(slot),
                              DASICS_LIBCFG_R | DASICS_LIBCFG_W, total);
    dynamic_data.library_buffer[0] = 'P';
    expected_data->library_buffer[0] = 'P';
    failures += run_access("LIBRARY-AFTER-FREE", 1, protected, 1, !protected,
                           dynamic_data.library_buffer, 'X', expected_data, &table, total);
    if (protected) {
        ret = dasics_libcfg_free(SLOT_COUNT);
        failures += record_table("LIBRARY-INVALID-HANDLE", &table, slot,
            (unsigned long)(long)ret, (unsigned long)-1L, EXC_DASICS_STORE_FAULT, start + 1, total);
        ret = dasics_libcfg_free(slot);
        failures += record_table("LIBRARY-DOUBLE-FREE", &table, slot,
            (unsigned long)(long)ret, 0, EXC_DASICS_STORE_FAULT, start + 1, total);
    }
    failures += record_value("TOTAL-TRAP-DELTA", dasics_n_extension_trap_count - start, protected ? 1 : 0, total);
    return failures;
}

static int restore(const struct owned_state *expected, const struct dynamic_data *data,
                    int registered)
{
    struct owned_state actual;
    if (registered)
        unregister_udasics();
    csr_write(CSR_DLCFG0, 0);
    for (unsigned int i = 0; i < SLOT_COUNT; i++)
        write_bound(i, expected->table.lo[i], expected->table.hi[i]);
    csr_write(CSR_DMAINCALL, expected->maincall);
    csr_write(CSR_DRETURNPC, expected->return_pc);
    csr_write(CSR_DFZRETURN, expected->active_return);
    csr_write(CSR_DFREASON, expected->freason);
    csr_write(CSR_USCRATCH, expected->scratch);
    csr_write(CSR_UTVEC, expected->vector);
    umaincall_helper = expected->helper;
    csr_write(CSR_DLCFG0, expected->table.cfg);
    copy_data(data);
    snapshot(&actual);
    int table_ok = table_equal(&actual.table, &expected->table);
    int data_ok = data_equal(data);
    int pass = !memcmp(&actual, expected, sizeof(actual)) && data_ok;
    printf(DYNAMIC_TAG " cleanup=STATE-RESTORE cfg=0x%lx expect_cfg=0x%lx "
           "table_ok=%d expect_table_ok=1 data_ok=%d expect_data_ok=1 "
           "maincall=0x%lx expect_maincall=0x%lx return_pc=0x%lx expect_return_pc=0x%lx "
           "active_return=0x%lx expect_active_return=0x%lx freason=0x%lx expect_freason=0x%lx "
           "uscratch=0x%lx expect_uscratch=0x%lx utvec=0x%lx expect_utvec=0x%lx "
           "helper=0x%lx expect_helper=0x%lx result=%s\n",
           actual.table.cfg, expected->table.cfg, table_ok, data_ok,
           actual.maincall, expected->maincall, actual.return_pc, expected->return_pc,
           actual.active_return, expected->active_return, actual.freason, expected->freason,
           actual.scratch, expected->scratch, actual.vector, expected->vector,
           actual.helper, expected->helper, pass ? "PASS" : "FAIL");
    return pass;
}

int main(int argc, char *argv[])
{
    int direct = argc == 2 && argv && argv[1] && !strcmp(argv[1], "--direct");
    int protected = direct || (argc == 2 && argv && argv[1] && !strcmp(argv[1], "--library-on"));
    if (!direct && !protected && !(argc == 2 && argv && argv[1] && !strcmp(argv[1], "--library-off"))) {
        fprintf(stderr, DYNAMIC_TAG " argument-contract argc=%d result=FAIL\n", argc);
        return 2;
    }
    const char *mode = direct ? "direct" : protected ? "library-on" : "library-off";
    struct owned_state original;
    struct dynamic_data original_data, expected_data;
    unsigned char *bytes = (unsigned char *)&original_data;
    const volatile unsigned char *actual = (const volatile unsigned char *)&dynamic_data;
    snapshot(&original);
    for (unsigned int i = 0; i < sizeof(original_data); i++)
        bytes[i] = actual[i];
    expected_data = original_data;
    unsigned long total = 0;
    int registered = 0;
    printf(DYNAMIC_TAG " begin mode=%s\n", mode);
    int failures = record_value("EXEC-UMAINCFG", dasics_linux_query_umaincfg(), protected ? DASICS_UCFG_ENA : 0, &total);
    if (!failures) {
        if (direct) {
            failures += run_direct(&expected_data, &original.table, &total);
        } else {
            register_udasics(0);
            registered = 1;
            failures += run_library(protected, &expected_data, &original.table, &total);
        }
    }
    /* Preserve N trap records and the runtime's post-trap USTATUS for its destructor. */
    if (!restore(&original, &original_data, registered))
        failures++;
    printf(DYNAMIC_TAG " summary mode=%s total=%lu failed=%d result=%s\n",
           mode, total, failures, failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
#else
int main(void)
{
    fprintf(stderr, DYNAMIC_TAG " requires Linux N-profile result=FAIL\n");
    return 2;
}
#endif
