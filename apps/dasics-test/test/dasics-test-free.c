#include <stdint.h>
#include <stdio.h>

#include "udasics.h"

#define DASICS_APP_FREE_TAG "[DASICS-APP-FREE]"
#define DASICS_APP_FREE_CASES 10UL
#define TEST_BUFFER_LEN 100UL

static char ATTR_ULIB_DATA freed_buffer[DASICS_BOUND_ALIGN_UP(TEST_BUFFER_LEN)]
    __attribute__((aligned(DASICS_BOUND_GRANULE))) = "dynamic bound data";

#pragma GCC push_options
#pragma GCC optimize("O0")
int ATTR_ULIB_TEXT free_after_release_store_operation(void)
{
    char value = 'X';

    asm volatile("sb %0, 0(%1)"
                 :
                 : "r"(value), "r"(&freed_buffer[0])
                 : "memory");
    return 0;
}
#pragma GCC pop_options

static int record_value_case(const char *name, uint64_t actual,
                             uint64_t expected)
{
    int pass = actual == expected;

    printf(DASICS_APP_FREE_TAG
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
    const uint32_t expected_cfg =
        DASICS_LIBCFG_V | DASICS_LIBCFG_R | DASICS_LIBCFG_W;
    uint32_t allocated_cfg = UINT32_MAX;
    uint32_t released_cfg = UINT32_MAX;
    uint64_t store_reason = 0;
    long pid = dasics_complete_app_getpid();
    int failures = 0;

    register_udasics(0);
    int32_t bound = dasics_libcfg_alloc(
        DASICS_LIBCFG_R | DASICS_LIBCFG_W,
        DASICS_BOUND_ALIGN_DOWN(freed_buffer),
        DASICS_BOUND_ALIGN_UP(freed_buffer + TEST_BUFFER_LEN));
    int setup_ok = bound >= 0;
    failures += record_bool_case("FREE-SETUP-BOUND-ALLOCATED", setup_ok);
    if (setup_ok) allocated_cfg = dasics_libcfg_get(bound);
    failures += record_value_case("FREE-SETUP-BOUND-CFG", allocated_cfg,
                                  expected_cfg);

    int32_t free_result = setup_ok ? dasics_libcfg_free(bound) : -1;
    failures += record_value_case("FREE-RELEASE-RETURN",
                                  (uint64_t)(int64_t)free_result, 0);
    if (setup_ok) released_cfg = dasics_libcfg_get(bound);
    failures += record_bool_case("FREE-RELEASE-VALID-BIT-CLEARED",
                                 (released_cfg & DASICS_LIBCFG_V) == 0);

    long cfg = dasics_complete_app_control(
        DASICS_COMPLETE_APP_CONTROL_SET, (uint64_t)pid,
        DASICS_COMPLETE_APP_STAGE_A, DASICS_COMPLETE_APP_CFG_OFF);
    failures += record_value_case("FREE-A-OS-CFG-OFF", (uint64_t)cfg,
                                  DASICS_COMPLETE_APP_CFG_OFF);
    freed_buffer[0] = 'P';
    if (setup_ok) free_after_release_store_operation();
    failures += record_bool_case("FREE-A-AFTER-RELEASE-STORE-SUCCEEDS",
                                 freed_buffer[0] == 'X');

    freed_buffer[0] = 'P';
    cfg = dasics_complete_app_control(
        DASICS_COMPLETE_APP_CONTROL_SET, (uint64_t)pid,
        DASICS_COMPLETE_APP_STAGE_B, DASICS_COMPLETE_APP_CFG_UENA);
    failures += record_value_case("FREE-B-OS-CFG-UENA", (uint64_t)cfg,
                                  DASICS_COMPLETE_APP_CFG_UENA);
    if (setup_ok) {
        lib_call(&free_after_release_store_operation);
        store_reason = csr_read(CSR_DFREASON);
    }
    failures += record_value_case("FREE-B-AFTER-RELEASE-STORE-REASON",
                                  store_reason, EXC_DASICS_STORE_FAULT);
    failures += record_bool_case("FREE-B-AFTER-RELEASE-STORE-NO-EFFECT",
                                 freed_buffer[0] == 'P');

    long restored = dasics_complete_app_control(
        DASICS_COMPLETE_APP_CONTROL_RESTORE, (uint64_t)pid,
        DASICS_COMPLETE_APP_STAGE_RESTORE, DASICS_COMPLETE_APP_CFG_UENA);
    failures += record_value_case("FREE-OS-CFG-RESTORED",
                                  (uint64_t)restored,
                                  DASICS_COMPLETE_APP_CFG_UENA);

    unregister_udasics();
    printf(DASICS_APP_FREE_TAG
           " summary scope=os-controlled-after-free-pair total=%lu failed=%d result=%s\n",
           DASICS_APP_FREE_CASES, failures,
           failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
