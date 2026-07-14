#include <stdint.h>
#include <stdio.h>

#include "udasics.h"

#define DASICS_APP_OFB_TAG "[DASICS-APP-OFB]"
#define DASICS_APP_OFB_CASES 10UL
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

int main(void)
{
    char load_expected = unbounded_data[0];
    char store_initial = unbounded_data[1];
    uint64_t load_reason = 0;
    uint64_t store_reason = 0;
    long pid = dasics_complete_app_getpid();
    int failures = 0;

    register_udasics(0);
    int32_t observation_bound = dasics_libcfg_alloc(
        DASICS_LIBCFG_R | DASICS_LIBCFG_W,
        DASICS_BOUND_ALIGN_DOWN(&load_observation),
        DASICS_BOUND_ALIGN_UP(&load_observation + 1));
    int setup_ok = observation_bound >= 0;
    failures += record_bool_case("OFB-SETUP-OBSERVATION-BOUND", setup_ok);

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

    if (observation_bound >= 0) dasics_libcfg_free(observation_bound);
    unregister_udasics();
    printf(DASICS_APP_OFB_TAG
           " summary scope=os-controlled-unbounded-pair total=%lu failed=%d result=%s\n",
           DASICS_APP_OFB_CASES, failures,
           failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
