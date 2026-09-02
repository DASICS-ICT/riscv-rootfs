#include <stdint.h>
#include <stdio.h>

#include "udasics.h"

#define DASICS_APP_RWX_TAG "[DASICS-APP-RWX]"
#if !DASICS_LINUX_DUAL_EXEC
#define DASICS_APP_RWX_CASES 14UL
#endif
#define LOAD_POISON UINT64_C(0xd15ea5ed5a17c0de)
#define TEST_BUFFER_LEN 100UL

static char ATTR_ULIB_DATA secret[DASICS_BOUND_ALIGN_UP(TEST_BUFFER_LEN)]
    __attribute__((aligned(DASICS_BOUND_GRANULE))) = "protected secret";
static char ATTR_ULIB_DATA pub_readonly[DASICS_BOUND_ALIGN_UP(TEST_BUFFER_LEN)]
    __attribute__((aligned(DASICS_BOUND_GRANULE))) = "read-only data";
static char ATTR_ULIB_DATA pub_rwbuffer[DASICS_BOUND_ALIGN_UP(TEST_BUFFER_LEN)]
    __attribute__((aligned(DASICS_BOUND_GRANULE))) = "read-write data";
static volatile uint64_t ATTR_ULIB_DATA secret_load_observation
    __attribute__((aligned(DASICS_BOUND_GRANULE)));

#pragma GCC push_options
#pragma GCC optimize("O0")
int ATTR_ULIB_TEXT rwx_allowed_operation(void)
{
    pub_rwbuffer[0] = pub_readonly[0];
    return 0;
}

int ATTR_ULIB_TEXT rwx_readonly_store_operation(void)
{
    char value = 'X';

    asm volatile("sb %0, 0(%1)"
                 :
                 : "r"(value), "r"(&pub_readonly[1])
                 : "memory");
    return 0;
}

int ATTR_ULIB_TEXT rwx_secret_load_operation(void)
{
    uintptr_t value = LOAD_POISON;

    asm volatile("lb %0, 0(%1)"
                 : "+&r"(value)
                 : "r"(&secret[2])
                 : "memory");
    secret_load_observation = value;
    return 0;
}

int ATTR_ULIB_TEXT rwx_secret_store_operation(void)
{
    char value = 'Y';

    asm volatile("sb %0, 0(%1)"
                 :
                 : "r"(value), "r"(&secret[3])
                 : "memory");
    return 0;
}
#pragma GCC pop_options

static int record_value_case(const char *name, uint64_t actual,
                             uint64_t expected)
{
    int pass = actual == expected;

    printf(DASICS_APP_RWX_TAG
           " case=%s actual=0x%lx expect=0x%lx result=%s\n",
           name, actual, expected, pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

static int record_bool_case(const char *name, int value)
{
    return record_value_case(name, value != 0, 1);
}

int main(int argc, char *argv[])
{
    char readonly_initial = pub_readonly[1];
    char secret_load_expected = secret[2];
    char secret_store_initial = secret[3];
    uint64_t readonly_reason = 0;
    uint64_t secret_load_reason = 0;
    uint64_t secret_store_reason = 0;
    int failures = 0;
#if DASICS_LINUX_DUAL_EXEC
    enum dasics_linux_exec_mode mode;
    unsigned long total;

    if (dasics_linux_parse_exec_mode(argc, argv, &mode)) {
        fprintf(stderr, DASICS_APP_RWX_TAG
                " argument-contract argc=%d result=FAIL\n", argc);
        return 2;
    }
    total = mode == DASICS_LINUX_EXEC_OFF ? 5UL : 9UL;
#else
    long pid = dasics_complete_app_getpid();

    (void)argc;
    (void)argv;
#endif

    register_udasics(0);
    int32_t readonly_bound = dasics_libcfg_alloc(
        DASICS_LIBCFG_R, DASICS_BOUND_ALIGN_DOWN(pub_readonly),
        DASICS_BOUND_ALIGN_UP(pub_readonly + TEST_BUFFER_LEN));
    int32_t rw_bound = dasics_libcfg_alloc(
        DASICS_LIBCFG_R | DASICS_LIBCFG_W,
        DASICS_BOUND_ALIGN_DOWN(pub_rwbuffer),
        DASICS_BOUND_ALIGN_UP(pub_rwbuffer + TEST_BUFFER_LEN));
    int32_t observation_bound = dasics_libcfg_alloc(
        DASICS_LIBCFG_R | DASICS_LIBCFG_W,
        DASICS_BOUND_ALIGN_DOWN(&secret_load_observation),
        DASICS_BOUND_ALIGN_UP(&secret_load_observation + 1));
    int setup_ok = readonly_bound >= 0 && rw_bound >= 0 &&
                   observation_bound >= 0;

    failures += record_bool_case("RWX-SETUP-BOUNDS-ALLOCATED", setup_ok);
#if DASICS_LINUX_DUAL_EXEC
    failures += record_value_case("RWX-EXEC-UMAINCFG",
                                  dasics_linux_query_umaincfg(),
                                  mode == DASICS_LINUX_EXEC_ON ?
                                      DASICS_UCFG_ENA : 0);

    if (mode == DASICS_LINUX_EXEC_OFF) {
        if (setup_ok) {
            rwx_readonly_store_operation();
            rwx_secret_load_operation();
            rwx_secret_store_operation();
        }
        failures += record_bool_case("RWX-A-READONLY-STORE-SUCCEEDS",
                                     pub_readonly[1] == 'X');
        failures += record_bool_case(
            "RWX-A-SECRET-LOAD-SUCCEEDS",
            secret_load_observation ==
                (uint64_t)(int64_t)secret_load_expected);
        failures += record_bool_case("RWX-A-SECRET-STORE-SUCCEEDS",
                                     secret[3] == 'Y');
    } else {
        secret_load_observation = LOAD_POISON;
        if (setup_ok) {
            pub_rwbuffer[0] = 0;
            lib_call(&rwx_allowed_operation);
            lib_call(&rwx_readonly_store_operation);
            readonly_reason = csr_read(CSR_DFREASON);
            lib_call(&rwx_secret_load_operation);
            secret_load_reason = csr_read(CSR_DFREASON);
            lib_call(&rwx_secret_store_operation);
            secret_store_reason = csr_read(CSR_DFREASON);
        }
        failures += record_bool_case("RWX-B-ALLOWED-RW-SUCCEEDS",
                                     pub_rwbuffer[0] == pub_readonly[0]);
        failures += record_value_case("RWX-B-READONLY-STORE-REASON",
                                      readonly_reason,
                                      EXC_DASICS_STORE_FAULT);
        failures += record_bool_case("RWX-B-READONLY-STORE-NO-EFFECT",
                                     pub_readonly[1] == readonly_initial);
        failures += record_value_case("RWX-B-SECRET-LOAD-REASON",
                                      secret_load_reason,
                                      EXC_DASICS_LOAD_FAULT);
        failures += record_value_case(
            "RWX-B-SECRET-LOAD-DESTINATION-POISON",
            secret_load_observation, LOAD_POISON);
        failures += record_value_case("RWX-B-SECRET-STORE-REASON",
                                      secret_store_reason,
                                      EXC_DASICS_STORE_FAULT);
        failures += record_bool_case("RWX-B-SECRET-STORE-NO-EFFECT",
                                     secret[3] == secret_store_initial);
    }
#else
    long cfg = dasics_complete_app_control(
        DASICS_COMPLETE_APP_CONTROL_SET, (uint64_t)pid,
        DASICS_COMPLETE_APP_STAGE_A, DASICS_COMPLETE_APP_CFG_OFF);
    failures += record_value_case("RWX-A-OS-CFG-OFF", (uint64_t)cfg,
                                  DASICS_COMPLETE_APP_CFG_OFF);

    if (setup_ok) {
        rwx_readonly_store_operation();
        rwx_secret_load_operation();
        rwx_secret_store_operation();
    }
    failures += record_bool_case("RWX-A-READONLY-STORE-SUCCEEDS",
                                 pub_readonly[1] == 'X');
    failures += record_bool_case("RWX-A-SECRET-LOAD-SUCCEEDS",
                                 secret_load_observation ==
                                     (uint64_t)(int64_t)secret_load_expected);
    failures += record_bool_case("RWX-A-SECRET-STORE-SUCCEEDS",
                                 secret[3] == 'Y');

    pub_readonly[1] = readonly_initial;
    secret[3] = secret_store_initial;
    secret_load_observation = LOAD_POISON;
    cfg = dasics_complete_app_control(
        DASICS_COMPLETE_APP_CONTROL_SET, (uint64_t)pid,
        DASICS_COMPLETE_APP_STAGE_B, DASICS_COMPLETE_APP_CFG_UENA);
    failures += record_value_case("RWX-B-OS-CFG-UENA", (uint64_t)cfg,
                                  DASICS_COMPLETE_APP_CFG_UENA);

    if (setup_ok) {
        pub_rwbuffer[0] = 0;
        lib_call(&rwx_allowed_operation);
        lib_call(&rwx_readonly_store_operation);
        readonly_reason = csr_read(CSR_DFREASON);
        lib_call(&rwx_secret_load_operation);
        secret_load_reason = csr_read(CSR_DFREASON);
        lib_call(&rwx_secret_store_operation);
        secret_store_reason = csr_read(CSR_DFREASON);
    }
    failures += record_bool_case("RWX-B-ALLOWED-RW-SUCCEEDS",
                                 pub_rwbuffer[0] == pub_readonly[0]);
    failures += record_value_case("RWX-B-READONLY-STORE-REASON",
                                  readonly_reason,
                                  EXC_DASICS_STORE_FAULT);
    failures += record_bool_case("RWX-B-READONLY-STORE-NO-EFFECT",
                                 pub_readonly[1] == readonly_initial);
    failures += record_value_case("RWX-B-SECRET-LOAD-REASON",
                                  secret_load_reason,
                                  EXC_DASICS_LOAD_FAULT);
    failures += record_value_case("RWX-B-SECRET-LOAD-DESTINATION-POISON",
                                  secret_load_observation, LOAD_POISON);
    failures += record_value_case("RWX-B-SECRET-STORE-REASON",
                                  secret_store_reason,
                                  EXC_DASICS_STORE_FAULT);
    failures += record_bool_case("RWX-B-SECRET-STORE-NO-EFFECT",
                                 secret[3] == secret_store_initial);

    long restored = dasics_complete_app_control(
        DASICS_COMPLETE_APP_CONTROL_RESTORE, (uint64_t)pid,
        DASICS_COMPLETE_APP_STAGE_RESTORE, DASICS_COMPLETE_APP_CFG_UENA);
    failures += record_value_case("RWX-OS-CFG-RESTORED",
                                  (uint64_t)restored,
                                  DASICS_COMPLETE_APP_CFG_UENA);
#endif

    if (readonly_bound >= 0) dasics_libcfg_free(readonly_bound);
    if (rw_bound >= 0) dasics_libcfg_free(rw_bound);
    if (observation_bound >= 0) dasics_libcfg_free(observation_bound);
    unregister_udasics();
#if DASICS_LINUX_DUAL_EXEC
    printf(DASICS_APP_RWX_TAG
           " summary scope=linux-exec-mode dasics=%s total=%lu failed=%d result=%s\n",
           dasics_linux_exec_mode_name(mode), total, failures,
           failures ? "FAIL" : "PASS");
#else
    printf(DASICS_APP_RWX_TAG
           " summary scope=os-controlled-rwx-pair total=%lu failed=%d result=%s\n",
           DASICS_APP_RWX_CASES, failures,
           failures ? "FAIL" : "PASS");
#endif
    return failures ? 1 : 0;
}
