#include <stdint.h>
#include <stdio.h>

#include "udasics.h"

#define DASICS_APP_JUMP_TAG "[DASICS-APP-JUMP]"
#define DASICS_APP_JUMP_CASES 7UL
#define JUMP_MARKER_POISON UINT64_C(0x4a554d50504f4953)
#define JUMP_MARKER_REACHED UINT64_C(0x4a554d5054414b45)

static volatile uint64_t ATTR_ULIB_DATA jump_marker
    __attribute__((aligned(DASICS_BOUND_GRANULE)));

static int __attribute__((noinline)) jump_main_target(void)
{
    jump_marker = JUMP_MARKER_REACHED;
    return 0;
}

#pragma GCC push_options
#pragma GCC optimize("O0")
int ATTR_ULIB_TEXT jump_to_main_operation(void)
{
    jump_main_target();
    return 0;
}
#pragma GCC pop_options

static int record_value_case(const char *name, uint64_t actual,
                             uint64_t expected)
{
    int pass = actual == expected;

    printf(DASICS_APP_JUMP_TAG
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
    uint64_t jump_reason = 0;
    long pid = dasics_complete_app_getpid();
    int failures = 0;

    register_udasics(0);
    int32_t marker_bound = dasics_libcfg_alloc(
        DASICS_LIBCFG_R | DASICS_LIBCFG_W,
        DASICS_BOUND_ALIGN_DOWN(&jump_marker),
        DASICS_BOUND_ALIGN_UP(&jump_marker + 1));
    int setup_ok = marker_bound >= 0;
    failures += record_bool_case("JUMP-SETUP-MARKER-BOUND", setup_ok);

    long cfg = dasics_complete_app_control(
        DASICS_COMPLETE_APP_CONTROL_SET, (uint64_t)pid,
        DASICS_COMPLETE_APP_STAGE_A, DASICS_COMPLETE_APP_CFG_OFF);
    failures += record_value_case("JUMP-A-OS-CFG-OFF", (uint64_t)cfg,
                                  DASICS_COMPLETE_APP_CFG_OFF);
    jump_marker = JUMP_MARKER_POISON;
    if (setup_ok) jump_to_main_operation();
    failures += record_value_case("JUMP-A-ULIB-TO-MAIN-SUCCEEDS",
                                  jump_marker, JUMP_MARKER_REACHED);

    jump_marker = JUMP_MARKER_POISON;
    cfg = dasics_complete_app_control(
        DASICS_COMPLETE_APP_CONTROL_SET, (uint64_t)pid,
        DASICS_COMPLETE_APP_STAGE_B, DASICS_COMPLETE_APP_CFG_UENA);
    failures += record_value_case("JUMP-B-OS-CFG-UENA", (uint64_t)cfg,
                                  DASICS_COMPLETE_APP_CFG_UENA);
    if (setup_ok) {
        lib_call(&jump_to_main_operation);
        jump_reason = csr_read(CSR_DFREASON);
    }
    failures += record_value_case("JUMP-B-ULIB-TO-MAIN-REASON",
                                  jump_reason, EXC_DASICS_JUMP_FAULT);
    failures += record_value_case("JUMP-B-MAIN-TARGET-NOT-REACHED",
                                  jump_marker, JUMP_MARKER_POISON);

    long restored = dasics_complete_app_control(
        DASICS_COMPLETE_APP_CONTROL_RESTORE, (uint64_t)pid,
        DASICS_COMPLETE_APP_STAGE_RESTORE, DASICS_COMPLETE_APP_CFG_UENA);
    failures += record_value_case("JUMP-OS-CFG-RESTORED",
                                  (uint64_t)restored,
                                  DASICS_COMPLETE_APP_CFG_UENA);

    if (marker_bound >= 0) dasics_libcfg_free(marker_bound);
    unregister_udasics();
    printf(DASICS_APP_JUMP_TAG
           " summary scope=os-controlled-ulib-main-pair total=%lu failed=%d result=%s\n",
           DASICS_APP_JUMP_CASES, failures,
           failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
