#include <stdint.h>
#include <stdio.h>

#include "udasics.h"

#define DASICS_APP_OFB_TAG "[DASICS-APP-OFB]"
#if !DASICS_LINUX_DUAL_EXEC
#define DASICS_APP_OFB_CASES 10UL
#endif
#define LOAD_POISON UINT64_C(0x0fb00fb00fb00fb0)
#define TEST_BUFFER_LEN 100UL

static char ATTR_ULIB_DATA unbounded_data[DASICS_BOUND_ALIGN_UP(TEST_BUFFER_LEN)]
    __attribute__((aligned(DASICS_BOUND_GRANULE))) = "unbounded data";
static volatile uint64_t ATTR_ULIB_DATA load_observation
    __attribute__((aligned(DASICS_BOUND_GRANULE)));

#pragma GCC push_options
#pragma GCC optimize("O0")
int ATTR_ULIB_TEXT ofb_load_operation(void)
{
    uintptr_t value = LOAD_POISON;

    asm volatile("lb %0, 0(%1)"
                 : "+&r"(value)
                 : "r"(&unbounded_data[0])
                 : "memory");
    load_observation = value;
    return 0;
}

int ATTR_ULIB_TEXT ofb_store_operation(void)
{
    char value = 'X';

    asm volatile("sb %0, 0(%1)"
                 :
                 : "r"(value), "r"(&unbounded_data[1])
                 : "memory");
    return 0;
}
#pragma GCC pop_options

static int record_value_case(const char *name, uint64_t actual,
                             uint64_t expected)
{
    int pass = actual == expected;

    printf(DASICS_APP_OFB_TAG
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
    char load_expected = unbounded_data[0];
    char store_initial = unbounded_data[1];
    uint64_t load_reason = 0;
    uint64_t store_reason = 0;
    int failures = 0;
#if DASICS_LINUX_DUAL_EXEC
    enum dasics_linux_exec_mode mode;
    unsigned long total;

    if (dasics_linux_parse_exec_mode(argc, argv, &mode)) {
        fprintf(stderr, DASICS_APP_OFB_TAG
                " argument-contract argc=%d result=FAIL\n", argc);
        return 2;
    }
    total = mode == DASICS_LINUX_EXEC_OFF ? 4UL : 6UL;
#else
    long pid = dasics_complete_app_getpid();

    (void)argc;
    (void)argv;
#endif

    register_udasics(0);
    int32_t observation_bound = dasics_libcfg_alloc(
        DASICS_LIBCFG_R | DASICS_LIBCFG_W,
        DASICS_BOUND_ALIGN_DOWN(&load_observation),
        DASICS_BOUND_ALIGN_UP(&load_observation + 1));
    int setup_ok = observation_bound >= 0;
    failures += record_bool_case("OFB-SETUP-OBSERVATION-BOUND", setup_ok);

#if DASICS_LINUX_DUAL_EXEC
    failures += record_value_case("OFB-EXEC-UMAINCFG",
                                  dasics_linux_query_umaincfg(),
                                  mode == DASICS_LINUX_EXEC_ON ?
                                      DASICS_UCFG_ENA : 0);
    load_observation = LOAD_POISON;
    if (mode == DASICS_LINUX_EXEC_OFF) {
        if (setup_ok) {
            ofb_load_operation();
            ofb_store_operation();
        }
        failures += record_value_case("OFB-A-UNBOUNDED-LOAD-SUCCEEDS",
                                      load_observation,
                                      (uint64_t)(int64_t)load_expected);
        failures += record_bool_case("OFB-A-UNBOUNDED-STORE-SUCCEEDS",
                                     unbounded_data[1] == 'X');
    } else {
        if (setup_ok) {
            lib_call(&ofb_load_operation);
            load_reason = csr_read(CSR_DFREASON);
            lib_call(&ofb_store_operation);
            store_reason = csr_read(CSR_DFREASON);
        }
        failures += record_value_case("OFB-B-UNBOUNDED-LOAD-REASON",
                                      load_reason,
                                      EXC_DASICS_LOAD_FAULT);
        failures += record_value_case("OFB-B-LOAD-DESTINATION-POISON",
                                      load_observation, LOAD_POISON);
        failures += record_value_case("OFB-B-UNBOUNDED-STORE-REASON",
                                      store_reason,
                                      EXC_DASICS_STORE_FAULT);
        failures += record_bool_case("OFB-B-UNBOUNDED-STORE-NO-EFFECT",
                                     unbounded_data[1] == store_initial);
    }
#else
    long cfg = dasics_complete_app_control(
        DASICS_COMPLETE_APP_CONTROL_SET, (uint64_t)pid,
        DASICS_COMPLETE_APP_STAGE_A, DASICS_COMPLETE_APP_CFG_OFF);
    failures += record_value_case("OFB-A-OS-CFG-OFF", (uint64_t)cfg,
                                  DASICS_COMPLETE_APP_CFG_OFF);
    load_observation = LOAD_POISON;
    if (setup_ok) {
        ofb_load_operation();
        ofb_store_operation();
    }
    failures += record_value_case("OFB-A-UNBOUNDED-LOAD-SUCCEEDS",
                                  load_observation,
                                  (uint64_t)(int64_t)load_expected);
    failures += record_bool_case("OFB-A-UNBOUNDED-STORE-SUCCEEDS",
                                 unbounded_data[1] == 'X');

    unbounded_data[1] = store_initial;
    load_observation = LOAD_POISON;
    cfg = dasics_complete_app_control(
        DASICS_COMPLETE_APP_CONTROL_SET, (uint64_t)pid,
        DASICS_COMPLETE_APP_STAGE_B, DASICS_COMPLETE_APP_CFG_UENA);
    failures += record_value_case("OFB-B-OS-CFG-UENA", (uint64_t)cfg,
                                  DASICS_COMPLETE_APP_CFG_UENA);
    if (setup_ok) {
        lib_call(&ofb_load_operation);
        load_reason = csr_read(CSR_DFREASON);
        lib_call(&ofb_store_operation);
        store_reason = csr_read(CSR_DFREASON);
    }
    failures += record_value_case("OFB-B-UNBOUNDED-LOAD-REASON",
                                  load_reason, EXC_DASICS_LOAD_FAULT);
    failures += record_value_case("OFB-B-LOAD-DESTINATION-POISON",
                                  load_observation, LOAD_POISON);
    failures += record_value_case("OFB-B-UNBOUNDED-STORE-REASON",
                                  store_reason, EXC_DASICS_STORE_FAULT);
    failures += record_bool_case("OFB-B-UNBOUNDED-STORE-NO-EFFECT",
                                 unbounded_data[1] == store_initial);

    long restored = dasics_complete_app_control(
        DASICS_COMPLETE_APP_CONTROL_RESTORE, (uint64_t)pid,
        DASICS_COMPLETE_APP_STAGE_RESTORE, DASICS_COMPLETE_APP_CFG_UENA);
    failures += record_value_case("OFB-OS-CFG-RESTORED",
                                  (uint64_t)restored,
                                  DASICS_COMPLETE_APP_CFG_UENA);
#endif

    if (observation_bound >= 0) dasics_libcfg_free(observation_bound);
    unregister_udasics();
#if DASICS_LINUX_DUAL_EXEC
    printf(DASICS_APP_OFB_TAG
           " summary scope=linux-exec-mode dasics=%s total=%lu failed=%d result=%s\n",
           dasics_linux_exec_mode_name(mode), total, failures,
           failures ? "FAIL" : "PASS");
#else
    printf(DASICS_APP_OFB_TAG
           " summary scope=os-controlled-unbounded-pair total=%lu failed=%d result=%s\n",
           DASICS_APP_OFB_CASES, failures,
           failures ? "FAIL" : "PASS");
#endif
    return failures ? 1 : 0;
}
