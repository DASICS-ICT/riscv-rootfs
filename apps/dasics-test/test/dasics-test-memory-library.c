#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "udasics.h"

#define MEMORY_LIBRARY_TAG "[DASICS-MEMORY-LIBRARY]"

#if DASICS_LINUX_DUAL_EXEC && defined(DASICS_N_EXTENSION_PROFILE)
#define TEST_BUFFER_LEN 100UL
#define BUFFER_BYTES DASICS_BOUND_ALIGN_UP(TEST_BUFFER_LEN)
#define LOAD_POISON UINT64_C(0xd15ea5ed5a17c0de)

static char ATTR_ULIB_DATA memory_library_secret[BUFFER_BYTES] = "protected secret";
static char ATTR_ULIB_DATA memory_library_readonly[BUFFER_BYTES] = "read-only data";
static char ATTR_ULIB_DATA memory_library_rwbuffer[BUFFER_BYTES] = "read-write data";

/* One library grant covers the real load destination and the callee's link
 * observation. The secret itself never receives a data-bound grant. */
static volatile struct {
    uint64_t load_value;
    uint64_t return_link;
} memory_library_observation ATTR_ULIB_DATA;

extern uint64_t umaincall_helper;
extern char memory_library_copy_recovery[];
extern char memory_library_readonly_store_site[], memory_library_readonly_store_recovery[];
extern char memory_library_secret_load_site[], memory_library_secret_load_recovery[];
extern char memory_library_secret_store_site[], memory_library_secret_store_recovery[];

#pragma GCC push_options
#pragma GCC optimize("O0")
static int __attribute__((noinline, used)) ATTR_ULIB_TEXT memory_library_copy(void)
{
    unsigned long link;

    memory_library_rwbuffer[0] = memory_library_readonly[0];
    asm volatile (
        ".balign 4\n.global memory_library_copy_recovery\n"
        "memory_library_copy_recovery:\nmv %0,ra\n"
        : "=r"(link) : : "ra", "memory");
    memory_library_observation.return_link = link;
    return 0;
}

static int __attribute__((noinline, used)) ATTR_ULIB_TEXT memory_library_readonly_store(void)
{
    unsigned long link;

    asm volatile (
        ".option push\n.option norvc\n.option norelax\n"
        "mv t0,%1\nli t1,88\n"
        ".global memory_library_readonly_store_site\nmemory_library_readonly_store_site:\n"
        "sb t1,0(t0)\n"
        ".global memory_library_readonly_store_recovery\nmemory_library_readonly_store_recovery:\n"
        "mv %0,ra\n.option pop\n"
        : "=r"(link) : "r"(&memory_library_readonly[1]) : "ra", "t0", "t1", "memory");
    memory_library_observation.return_link = link;
    return 0;
}

static int __attribute__((noinline, used)) ATTR_ULIB_TEXT memory_library_secret_load(void)
{
    unsigned long value, link;

    asm volatile (
        ".option push\n.option norvc\n.option norelax\n"
        "mv t0,%2\nli t2,0xd15ea5ed5a17c0de\n"
        ".global memory_library_secret_load_site\nmemory_library_secret_load_site:\n"
        "lb t2,0(t0)\n"
        ".global memory_library_secret_load_recovery\nmemory_library_secret_load_recovery:\n"
        "mv %0,t2\nmv %1,ra\n.option pop\n"
        : "=r"(value), "=r"(link) : "r"(&memory_library_secret[2])
        : "ra", "t0", "t2", "memory");
    memory_library_observation.load_value = value;
    memory_library_observation.return_link = link;
    return 0;
}

static int __attribute__((noinline, used)) ATTR_ULIB_TEXT memory_library_secret_store(void)
{
    unsigned long link;

    asm volatile (
        ".option push\n.option norvc\n.option norelax\n"
        "mv t0,%1\nli t1,89\n"
        ".global memory_library_secret_store_site\nmemory_library_secret_store_site:\n"
        "sb t1,0(t0)\n"
        ".global memory_library_secret_store_recovery\nmemory_library_secret_store_recovery:\n"
        "mv %0,ra\n.option pop\n"
        : "=r"(link) : "r"(&memory_library_secret[3]) : "ra", "t0", "t1", "memory");
    memory_library_observation.return_link = link;
    return 0;
}
#pragma GCC pop_options

#define MEMORY_BOUND_LIST(M) \
    M(0, 0x890, 0x891) M(1, 0x892, 0x893) M(2, 0x894, 0x895) M(3, 0x896, 0x897) \
    M(4, 0x898, 0x899) M(5, 0x89a, 0x89b) M(6, 0x89c, 0x89d) M(7, 0x89e, 0x89f) \
    M(8, 0x8a0, 0x8a1) M(9, 0x8a2, 0x8a3) M(10, 0x8a4, 0x8a5) M(11, 0x8a6, 0x8a7) \
    M(12, 0x8a8, 0x8a9) M(13, 0x8aa, 0x8ab) M(14, 0x8ac, 0x8ad) M(15, 0x8ae, 0x8af)

struct library_state {
    uint64_t libcfg, lo[16], hi[16];
    uint64_t maincall, return_pc, active_return, freason, uscratch, utvec, helper;
};

struct library_data {
    char secret[BUFFER_BYTES], readonly[BUFFER_BYTES], rwbuffer[BUFFER_BYTES];
    uint64_t load_value, return_link;
};

static void read_library_state(struct library_state *state)
{
    state->libcfg = csr_read(CSR_DLCFG0);
#define READ_BOUND(index, lo_csr, hi_csr) \
    state->lo[index] = csr_read(lo_csr); state->hi[index] = csr_read(hi_csr);
    MEMORY_BOUND_LIST(READ_BOUND)
#undef READ_BOUND
    state->maincall = csr_read(CSR_DMAINCALL);
    state->return_pc = csr_read(CSR_DRETURNPC);
    state->active_return = csr_read(CSR_DFZRETURN);
    state->freason = csr_read(CSR_DFREASON);
    state->uscratch = csr_read(CSR_USCRATCH);
    state->utvec = csr_read(CSR_UTVEC);
    state->helper = umaincall_helper;
}

static void read_library_data(struct library_data *data)
{
    memcpy(data->secret, memory_library_secret, BUFFER_BYTES);
    memcpy(data->readonly, memory_library_readonly, BUFFER_BYTES);
    memcpy(data->rwbuffer, memory_library_rwbuffer, BUFFER_BYTES);
    data->load_value = memory_library_observation.load_value;
    data->return_link = memory_library_observation.return_link;
}

static int buffers_match(const struct library_data *expected)
{
    return !memcmp(memory_library_secret, expected->secret, BUFFER_BYTES) &&
           !memcmp(memory_library_readonly, expected->readonly, BUFFER_BYTES) &&
           !memcmp(memory_library_rwbuffer, expected->rwbuffer, BUFFER_BYTES) &&
           memory_library_observation.load_value == expected->load_value;
}

static int record_value(const char *name, uint64_t actual, uint64_t expected,
                         unsigned long *total)
{
    int pass = actual == expected;

    (*total)++;
    printf(MEMORY_LIBRARY_TAG " case=%s actual=0x%lx expect=0x%lx result=%s\n",
           name, actual, expected, pass ? "PASS" : "FAIL");
    return !pass;
}

static int allocation_matches(int index, unsigned long access, const void *base,
                                unsigned long size)
{
    struct library_state actual;

    if (index < 0 || index >= DASICS_LIBCFG_WIDTH)
        return 0;
    read_library_state(&actual);
    return dasics_libcfg_get(index) == (DASICS_LIBCFG_V | access) &&
           actual.lo[index] == DASICS_BOUND_ALIGN_DOWN(base) &&
           actual.hi[index] == DASICS_BOUND_ALIGN_UP((uintptr_t)base + size);
}

static int record_fault_context(const char *name, unsigned long index,
    unsigned long after, char *site, unsigned long address,
    unsigned long reason, char *recovery, unsigned long return_link, unsigned long *total)
{
    int available = index < DASICS_N_EXTENSION_TRAP_RECORDS && after == index + 1;
    unsigned long cause = 0, status = 0, pc = 0, tval = 0, actual_reason = 0;
    unsigned long resume = 0, scratch = 0, vector = 0, source = 0, gpr = 0, ra = 0;
    unsigned long expected_gpr = reason == EXC_DASICS_LOAD_FAULT ? LOAD_POISON :
                                 (address == (uintptr_t)&memory_library_readonly[1] ? 'X' : 'Y');

    if (available) {
        const volatile dasics_n_extension_trap_record_t *record = &dasics_n_extension_trap_records[index];
        cause = record->ucause; status = record->ustatus; pc = record->uepc;
        tval = record->utval; actual_reason = record->dfreason; resume = record->recovery;
        scratch = record->uscratch; vector = record->utvec; source = record->gpr[5];
        gpr = record->gpr[reason == EXC_DASICS_LOAD_FAULT ? 7 : 6];
        ra = record->gpr[1];
    }
    unsigned long post_status = csr_read(CSR_USTATUS);
    int pass = available && cause == DASICS_N_EXTENSION_UCHECK_CAUSE && status == 0x10 &&
        pc == (uintptr_t)site && tval == address && actual_reason == reason &&
        resume == (uintptr_t)recovery && scratch == (uintptr_t)recovery &&
        vector == (uintptr_t)dasics_ufault_entry && source == address && gpr == expected_gpr &&
        ra == return_link && post_status == 0x11;

    (*total)++;
    printf(MEMORY_LIBRARY_TAG " case=%s-U24-CONTEXT sequence=%lu actual=0x%lx expect=0x18 "
           "ustatus=0x%lx expect_ustatus=0x10 uepc=0x%lx expect_uepc=0x%lx "
           "utval=0x%lx expect_utval=0x%lx freason=0x%lx expect_freason=0x%lx "
           "recovery=0x%lx expect_recovery=0x%lx uscratch=0x%lx expect_uscratch=0x%lx "
           "utvec=0x%lx expect_utvec=0x%lx source=0x%lx expect_source=0x%lx "
           "gpr=0x%lx expect_gpr=0x%lx ra=0x%lx expect_ra=0x%lx "
           "post_ustatus=0x%lx expect_post_ustatus=0x11 result=%s\n",
           name, index + 1, cause, status, pc, (uintptr_t)site, tval, address,
           actual_reason, reason, resume, (uintptr_t)recovery, scratch, (uintptr_t)recovery,
           vector, (uintptr_t)dasics_ufault_entry, source, address, gpr, expected_gpr,
           ra, return_link, post_status, pass ? "PASS" : "FAIL");
    return !pass;
}

static int run_operation(unsigned int operation, int protected,
                           struct library_data *expected, unsigned long *total)
{
    static int (*const functions[])(void) = {memory_library_copy, memory_library_readonly_store,
                                           memory_library_secret_load, memory_library_secret_store};
    static const char *const names[] = {"ALLOW-COPY", "READONLY-STORE", "SECRET-LOAD", "SECRET-STORE"};
    char *const sites[] = {NULL, memory_library_readonly_store_site,
                           memory_library_secret_load_site, memory_library_secret_store_site};
    char *const recoveries[] = {memory_library_copy_recovery, memory_library_readonly_store_recovery,
                               memory_library_secret_load_recovery, memory_library_secret_store_recovery};
    unsigned long addresses[] = {0, (uintptr_t)&memory_library_readonly[1],
                                 (uintptr_t)&memory_library_secret[2], (uintptr_t)&memory_library_secret[3]};
    unsigned long reasons[] = {0, EXC_DASICS_STORE_FAULT, EXC_DASICS_LOAD_FAULT, EXC_DASICS_STORE_FAULT};
    unsigned long before = dasics_n_extension_trap_count;
    unsigned long old_return_pc = csr_read(CSR_DRETURNPC);
    int denied = protected && operation != 0;
    char name[80];
    int failures = 0;

    if (operation == 0)
        expected->rwbuffer[0] = expected->readonly[0];
    else if (!protected && operation == 1)
        expected->readonly[1] = 'X';
    else if (!protected && operation == 2)
        expected->load_value = (uint64_t)(int64_t)(signed char)expected->secret[2];
    else if (!protected && operation == 3)
        expected->secret[3] = 'Y';

    csr_write(CSR_DFREASON, 0);
    csr_write(CSR_USCRATCH, (uintptr_t)recoveries[operation]);
    int call_result = protected ? lib_call(functions[operation]) : functions[operation]();
    csr_write(CSR_USCRATCH, 0);
    unsigned long reason = csr_read(CSR_DFREASON), after = dasics_n_extension_trap_count;
    unsigned long return_pc = csr_read(CSR_DRETURNPC);
    unsigned long link = memory_library_observation.return_link;
    unsigned long expected_pc = protected ? link : old_return_pc;
    unsigned long actual = operation == 0 ? (unsigned char)memory_library_rwbuffer[0] :
        operation == 1 ? (unsigned char)memory_library_readonly[1] :
        operation == 2 ? memory_library_observation.load_value : (unsigned char)memory_library_secret[3];
    unsigned long wanted = operation == 0 ? (unsigned char)expected->rwbuffer[0] :
        operation == 1 ? (unsigned char)expected->readonly[1] :
        operation == 2 ? expected->load_value : (unsigned char)expected->secret[3];

    snprintf(name, sizeof(name), "%s-VALUE", names[operation]);
    failures += record_value(name, actual, wanted, total);
    snprintf(name, sizeof(name), "%s-FREASON", names[operation]);
    failures += record_value(name, reason, denied ? reasons[operation] : 0, total);
    int control_ok = after == before + denied && call_result == 0 && link != 0 &&
                     return_pc == expected_pc && csr_read(CSR_USCRATCH) == 0;
    (*total)++;
    printf(MEMORY_LIBRARY_TAG " case=%s-CONTROL actual=0x%lx expect=0x%lx call_result=%d "
           "expect_call_result=0 traps=%lu->%lu expect_delta=%d link=0x%lx result=%s\n",
           names[operation], return_pc, expected_pc, call_result, before, after, denied, link,
           control_ok ? "PASS" : "FAIL");
    failures += !control_ok;
    snprintf(name, sizeof(name), "%s-BUFFERS", names[operation]);
    failures += record_value(name, buffers_match(expected), 1, total);
    if (denied)
        failures += record_fault_context(names[operation], before, after, sites[operation],
            addresses[operation], reasons[operation], recoveries[operation], link, total);
    return failures;
}

static int release_bound(const char *name, int index, unsigned long *total)
{
    unsigned long before = csr_read(CSR_DLCFG0);
    int ret = dasics_libcfg_free(index);
    unsigned long after = csr_read(CSR_DLCFG0);
    unsigned long expected = before & ~(DASICS_LIBCFG_V << (4 * index));
    int pass = ret == 0 && after == expected;

    (*total)++;
    printf(MEMORY_LIBRARY_TAG " case=FREE-%s actual=0x%lx expect=0x0 slot=%d "
           "cfg=0x%lx expect_cfg=0x%lx result=%s\n",
           name, (unsigned long)(long)ret, index, after, expected, pass ? "PASS" : "FAIL");
    return !pass;
}

static int restore_state(const struct library_state *expected, const struct library_data *data,
                          const int bounds[3])
{
    struct library_state actual;

    unregister_udasics();
    csr_write(CSR_DLCFG0, 0);
    for (unsigned int i = 0; i < 3; i++) {
        switch (bounds[i]) {
#define RESTORE_BOUND(index, lo_csr, hi_csr) case index: \
            csr_write(lo_csr, expected->lo[index]); csr_write(hi_csr, expected->hi[index]); break;
            MEMORY_BOUND_LIST(RESTORE_BOUND)
#undef RESTORE_BOUND
        }
    }
    csr_write(CSR_DMAINCALL, expected->maincall);
    csr_write(CSR_DRETURNPC, expected->return_pc);
    csr_write(CSR_DFZRETURN, expected->active_return);
    csr_write(CSR_DFREASON, expected->freason);
    csr_write(CSR_USCRATCH, expected->uscratch);
    csr_write(CSR_UTVEC, expected->utvec);
    umaincall_helper = expected->helper;
    csr_write(CSR_DLCFG0, expected->libcfg);
    memcpy(memory_library_secret, data->secret, BUFFER_BYTES);
    memcpy(memory_library_readonly, data->readonly, BUFFER_BYTES);
    memcpy(memory_library_rwbuffer, data->rwbuffer, BUFFER_BYTES);
    memory_library_observation.load_value = data->load_value;
    memory_library_observation.return_link = data->return_link;
    read_library_state(&actual);
    int bounds_ok = !memcmp(actual.lo, expected->lo, sizeof(actual.lo)) &&
                    !memcmp(actual.hi, expected->hi, sizeof(actual.hi));
    int data_ok = buffers_match(data) && memory_library_observation.return_link == data->return_link;
    int pass = !memcmp(&actual, expected, sizeof(actual)) && data_ok;

    for (unsigned int i = 0; i < 3; i++) {
        int slot = bounds[i];
        if (slot >= 0 && slot < DASICS_LIBCFG_WIDTH)
            printf(MEMORY_LIBRARY_TAG " cleanup=BOUND-RESTORE slot=%d lo=0x%lx expect_lo=0x%lx "
                   "hi=0x%lx expect_hi=0x%lx result=%s\n", slot, actual.lo[slot], expected->lo[slot],
                   actual.hi[slot], expected->hi[slot],
                   actual.lo[slot] == expected->lo[slot] && actual.hi[slot] == expected->hi[slot] ? "PASS" : "FAIL");
    }
    printf(MEMORY_LIBRARY_TAG " cleanup=STATE-RESTORE libcfg=0x%lx expect_libcfg=0x%lx "
           "maincall=0x%lx expect_maincall=0x%lx return_pc=0x%lx expect_return_pc=0x%lx "
           "active_return=0x%lx expect_active_return=0x%lx freason=0x%lx expect_freason=0x%lx "
           "uscratch=0x%lx expect_uscratch=0x%lx utvec=0x%lx expect_utvec=0x%lx "
           "helper=0x%lx expect_helper=0x%lx bounds_ok=%d expect_bounds_ok=1 "
           "data_ok=%d expect_data_ok=1 result=%s\n",
           actual.libcfg, expected->libcfg, actual.maincall, expected->maincall,
           actual.return_pc, expected->return_pc, actual.active_return, expected->active_return,
           actual.freason, expected->freason, actual.uscratch, expected->uscratch,
           actual.utvec, expected->utvec, actual.helper, expected->helper,
           bounds_ok, data_ok, pass ? "PASS" : "FAIL");
    return pass;
}

int main(int argc, char *argv[])
{
    enum dasics_linux_exec_mode mode;
    struct library_state original;
    struct library_data original_data, expected;
    static const char *const bound_names[] = {"READONLY", "RWBUFFER", "OBSERVATION"};
    int bounds[3] = {-1, -1, -1};
    unsigned long total = 0;
    int failures = 0;

    if (dasics_linux_parse_exec_mode(argc, argv, &mode)) {
        fprintf(stderr, MEMORY_LIBRARY_TAG " argument-contract argc=%d result=FAIL\n", argc);
        return 2;
    }
    int protected = mode == DASICS_LINUX_EXEC_ON;
    read_library_state(&original);
    read_library_data(&original_data);
    expected = original_data;
    unsigned long start_count = dasics_n_extension_trap_count;
    printf(MEMORY_LIBRARY_TAG " begin scope=linux-exec-mode dasics=%s buffer_bytes=%lu\n",
           dasics_linux_exec_mode_name(mode), (unsigned long)BUFFER_BYTES);
    failures += record_value("EXEC-UMAINCFG", dasics_linux_query_umaincfg(), protected ? DASICS_UCFG_ENA : 0, &total);
    if (failures)
        goto cleanup;
    register_udasics(0);
    bounds[0] = dasics_libcfg_alloc(DASICS_LIBCFG_R, DASICS_BOUND_ALIGN_DOWN(memory_library_readonly),
        DASICS_BOUND_ALIGN_UP(memory_library_readonly + TEST_BUFFER_LEN));
    bounds[1] = dasics_libcfg_alloc(DASICS_LIBCFG_R | DASICS_LIBCFG_W, DASICS_BOUND_ALIGN_DOWN(memory_library_rwbuffer),
        DASICS_BOUND_ALIGN_UP(memory_library_rwbuffer + TEST_BUFFER_LEN));
    bounds[2] = dasics_libcfg_alloc(DASICS_LIBCFG_R | DASICS_LIBCFG_W, DASICS_BOUND_ALIGN_DOWN(&memory_library_observation),
        DASICS_BOUND_ALIGN_UP(&memory_library_observation + 1));
    int setup_ok = allocation_matches(bounds[0], DASICS_LIBCFG_R, memory_library_readonly, TEST_BUFFER_LEN) &&
        allocation_matches(bounds[1], DASICS_LIBCFG_R | DASICS_LIBCFG_W, memory_library_rwbuffer, TEST_BUFFER_LEN) &&
        allocation_matches(bounds[2], DASICS_LIBCFG_R | DASICS_LIBCFG_W,
            (const void *)&memory_library_observation, sizeof(memory_library_observation));
    failures += record_value("SETUP-BOUNDS-ALLOCATED", setup_ok, 1, &total);
    if (!setup_ok)
        goto cleanup;

    memory_library_rwbuffer[0] = 0;
    memory_library_observation.load_value = LOAD_POISON;
    expected.rwbuffer[0] = 0;
    expected.load_value = LOAD_POISON;
    for (unsigned int operation = 0; operation < 4; operation++)
        failures += run_operation(operation, protected, &expected, &total);
    failures += record_value("TOTAL-TRAP-DELTA", dasics_n_extension_trap_count - start_count,
                              protected ? 3 : 0, &total);
cleanup:
    for (unsigned int i = 0; i < 3; i++) {
        if (bounds[i] >= 0 && bounds[i] < DASICS_LIBCFG_WIDTH)
            failures += release_bound(bound_names[i], bounds[i], &total);
    }
    /* Keep USTATUS and all N trap records intact for the shared destructor. */
    if (!restore_state(&original, &original_data, bounds))
        failures++;
    printf(MEMORY_LIBRARY_TAG " summary scope=linux-exec-mode dasics=%s total=%lu failed=%d result=%s\n",
           dasics_linux_exec_mode_name(mode), total, failures, failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
#else
int main(void)
{
    fprintf(stderr, MEMORY_LIBRARY_TAG " requires Linux N-profile result=FAIL\n");
    return 2;
}
#endif
