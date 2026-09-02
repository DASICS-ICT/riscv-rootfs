#include <stdint.h>
#include <stdio.h>
#include <machine/syscall.h>

#include "udasics.h"

#define DASICS_APP_SYSCALL_TAG "[DASICS-APP-SYSCALL]"
#if !DASICS_LINUX_DUAL_EXEC
#define DASICS_APP_SYSCALL_CASES 11UL
#endif
#define SYSCALL_OUTPUT_LEN 8UL
#define SYSCALL_GAP_LEN 24UL
#define SYSCALL_STDOUT_FD 1L

static char ATTR_ULIB_DATA attack_output[SYSCALL_OUTPUT_LEN]
    __attribute__((aligned(DASICS_BOUND_GRANULE))) =
        {'P', 'A', 'I', 'R', '-', 'A', '!', '\n'};
static char ATTR_ULIB_DATA allowed_output_one[SYSCALL_OUTPUT_LEN]
    __attribute__((aligned(DASICS_BOUND_GRANULE))) =
        {'A', 'L', 'L', 'O', 'W', '-', '1', '\n'};
static char ATTR_ULIB_DATA allowed_output_two[SYSCALL_OUTPUT_LEN]
    __attribute__((aligned(DASICS_BOUND_GRANULE))) =
        {'A', 'L', 'L', 'O', 'W', '-', '2', '\n'};
static char ATTR_ULIB_DATA gap_output[SYSCALL_GAP_LEN]
    __attribute__((aligned(DASICS_BOUND_GRANULE))) =
        {'G', 'A', 'P', '-', 'O', 'U', 'T', '!',
         'N', 'O', 'T', '-', 'A', 'L', 'L', 'O',
         'W', 'E', 'D', '-', 'H', 'E', 'R', 'E'};

struct syscall_observation_values {
    long attack_return;
    long allowed_one_return;
    long allowed_two_return;
    long gap_return;
};

static volatile struct syscall_observation_values ATTR_ULIB_DATA
    syscall_observations __attribute__((aligned(DASICS_BOUND_GRANULE)));

#pragma GCC push_options
#pragma GCC optimize("O0")
int ATTR_ULIB_TEXT syscall_attack_write_operation(void)
{
    syscall_observations.attack_return =
        ulib_write(SYSCALL_STDOUT_FD, attack_output, SYSCALL_OUTPUT_LEN);
    return 0;
}

int ATTR_ULIB_TEXT syscall_allowed_one_operation(void)
{
    syscall_observations.allowed_one_return =
        ulib_write(SYSCALL_STDOUT_FD, allowed_output_one,
                   SYSCALL_OUTPUT_LEN);
    return 0;
}

int ATTR_ULIB_TEXT syscall_allowed_two_operation(void)
{
    syscall_observations.allowed_two_return =
        ulib_write(SYSCALL_STDOUT_FD, allowed_output_two,
                   SYSCALL_OUTPUT_LEN);
    return 0;
}

int ATTR_ULIB_TEXT syscall_gap_operation(void)
{
    syscall_observations.gap_return =
        ulib_write(SYSCALL_STDOUT_FD, gap_output, SYSCALL_GAP_LEN);
    return 0;
}
#pragma GCC pop_options

static int record_value_case(const char *name, uint64_t actual,
                             uint64_t expected)
{
    int pass = actual == expected;

    printf(DASICS_APP_SYSCALL_TAG
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
    uint64_t attack_reason = 0;
    uint64_t gap_reason = 0;
    int failures = 0;
#if DASICS_LINUX_DUAL_EXEC
    enum dasics_linux_exec_mode mode;
    unsigned long total;

    if (dasics_linux_parse_exec_mode(argc, argv, &mode)) {
        fprintf(stderr, DASICS_APP_SYSCALL_TAG
                " argument-contract argc=%d result=FAIL\n", argc);
        return 2;
    }
    total = mode == DASICS_LINUX_EXEC_OFF ? 3UL : 8UL;
#else
    long pid = dasics_complete_app_getpid();

    (void)argc;
    (void)argv;
#endif

    register_udasics(0);
    int32_t observation_bound = dasics_libcfg_alloc(
        DASICS_LIBCFG_R | DASICS_LIBCFG_W,
        DASICS_BOUND_ALIGN_DOWN(&syscall_observations),
        DASICS_BOUND_ALIGN_UP(&syscall_observations + 1));
    // Each allowed output has one exact architectural [base, base + 8) bound.
    int32_t allowed_one_bound = dasics_libcfg_alloc(
        DASICS_LIBCFG_R, (uint64_t)allowed_output_one,
        (uint64_t)(allowed_output_one + SYSCALL_OUTPUT_LEN));
    int32_t allowed_two_bound = dasics_libcfg_alloc(
        DASICS_LIBCFG_R, (uint64_t)allowed_output_two,
        (uint64_t)(allowed_output_two + SYSCALL_OUTPUT_LEN));
    // The middle eight bytes remain outside both ranges.
    int32_t gap_low_bound = dasics_libcfg_alloc(
        DASICS_LIBCFG_R, (uint64_t)gap_output,
        (uint64_t)(gap_output + DASICS_BOUND_GRANULE));
    int32_t gap_high_bound = dasics_libcfg_alloc(
        DASICS_LIBCFG_R,
        (uint64_t)(gap_output + 2 * DASICS_BOUND_GRANULE),
        (uint64_t)(gap_output + SYSCALL_GAP_LEN));
    int setup_ok = observation_bound >= 0 && allowed_one_bound >= 0 &&
                   allowed_two_bound >= 0 && gap_low_bound >= 0 &&
                   gap_high_bound >= 0;
    failures += record_bool_case("SYSCALL-SETUP-BOUNDS-ALLOCATED", setup_ok);

#if DASICS_LINUX_DUAL_EXEC
    failures += record_value_case("SYSCALL-EXEC-UMAINCFG",
                                  dasics_linux_query_umaincfg(),
                                  mode == DASICS_LINUX_EXEC_ON ?
                                      DASICS_UCFG_ENA : 0);
    syscall_observations.attack_return = -1;
    if (mode == DASICS_LINUX_EXEC_OFF) {
        if (setup_ok) syscall_attack_write_operation();
        failures += record_value_case(
            "SYSCALL-A-UNBOUNDED-WRITE-RETURNS-LEN",
            syscall_observations.attack_return, SYSCALL_OUTPUT_LEN);
    } else {
        syscall_observations.allowed_one_return = -1;
        syscall_observations.allowed_two_return = -1;
        syscall_observations.gap_return = -1;
        if (setup_ok) {
            lib_call(&syscall_attack_write_operation);
            attack_reason = csr_read(CSR_DFREASON);
            lib_call(&syscall_allowed_one_operation);
            lib_call(&syscall_allowed_two_operation);
            lib_call(&syscall_gap_operation);
            gap_reason = csr_read(CSR_DFREASON);
        }
        failures += record_value_case(
            "SYSCALL-B-UNBOUNDED-WRITE-REASON", attack_reason,
            EXC_DASICS_ECALL_FAULT);
        failures += record_value_case(
            "SYSCALL-B-UNBOUNDED-WRITE-A0-PRESERVED",
            syscall_observations.attack_return, SYSCALL_STDOUT_FD);
        failures += record_value_case(
            "SYSCALL-B-ALLOWED-ONE-RETURNS-LEN",
            syscall_observations.allowed_one_return, SYSCALL_OUTPUT_LEN);
        failures += record_value_case(
            "SYSCALL-B-ALLOWED-TWO-RETURNS-LEN",
            syscall_observations.allowed_two_return, SYSCALL_OUTPUT_LEN);
        failures += record_value_case("SYSCALL-B-GAP-WRITE-REASON",
                                      gap_reason,
                                      EXC_DASICS_ECALL_FAULT);
        failures += record_value_case(
            "SYSCALL-B-GAP-WRITE-A0-PRESERVED",
            syscall_observations.gap_return, SYSCALL_STDOUT_FD);
    }
#else
    long cfg = dasics_complete_app_control(
        DASICS_COMPLETE_APP_CONTROL_SET, (uint64_t)pid,
        DASICS_COMPLETE_APP_STAGE_A, DASICS_COMPLETE_APP_CFG_OFF);
    failures += record_value_case("SYSCALL-A-OS-CFG-OFF", (uint64_t)cfg,
                                  DASICS_COMPLETE_APP_CFG_OFF);
    syscall_observations.attack_return = -1;
    if (setup_ok) syscall_attack_write_operation();
    failures += record_value_case("SYSCALL-A-UNBOUNDED-WRITE-RETURNS-LEN",
                                  syscall_observations.attack_return,
                                  SYSCALL_OUTPUT_LEN);

    syscall_observations.attack_return = -1;
    syscall_observations.allowed_one_return = -1;
    syscall_observations.allowed_two_return = -1;
    syscall_observations.gap_return = -1;
    cfg = dasics_complete_app_control(
        DASICS_COMPLETE_APP_CONTROL_SET, (uint64_t)pid,
        DASICS_COMPLETE_APP_STAGE_B, DASICS_COMPLETE_APP_CFG_UENA);
    failures += record_value_case("SYSCALL-B-OS-CFG-UENA", (uint64_t)cfg,
                                  DASICS_COMPLETE_APP_CFG_UENA);
    if (setup_ok) {
        lib_call(&syscall_attack_write_operation);
        attack_reason = csr_read(CSR_DFREASON);
        lib_call(&syscall_allowed_one_operation);
        lib_call(&syscall_allowed_two_operation);
        lib_call(&syscall_gap_operation);
        gap_reason = csr_read(CSR_DFREASON);
    }
    failures += record_value_case("SYSCALL-B-UNBOUNDED-WRITE-REASON",
                                  attack_reason,
                                  EXC_DASICS_ECALL_FAULT);
    failures += record_value_case("SYSCALL-B-UNBOUNDED-WRITE-A0-PRESERVED",
                                  syscall_observations.attack_return,
                                  SYSCALL_STDOUT_FD);
    failures += record_value_case("SYSCALL-B-ALLOWED-ONE-RETURNS-LEN",
                                  syscall_observations.allowed_one_return,
                                  SYSCALL_OUTPUT_LEN);
    failures += record_value_case("SYSCALL-B-ALLOWED-TWO-RETURNS-LEN",
                                  syscall_observations.allowed_two_return,
                                  SYSCALL_OUTPUT_LEN);
    failures += record_value_case("SYSCALL-B-GAP-WRITE-REASON", gap_reason,
                                  EXC_DASICS_ECALL_FAULT);
    failures += record_value_case("SYSCALL-B-GAP-WRITE-A0-PRESERVED",
                                  syscall_observations.gap_return,
                                  SYSCALL_STDOUT_FD);

    long restored = dasics_complete_app_control(
        DASICS_COMPLETE_APP_CONTROL_RESTORE, (uint64_t)pid,
        DASICS_COMPLETE_APP_STAGE_RESTORE, DASICS_COMPLETE_APP_CFG_UENA);
    failures += record_value_case("SYSCALL-OS-CFG-RESTORED",
                                  (uint64_t)restored,
                                  DASICS_COMPLETE_APP_CFG_UENA);
#endif

    if (observation_bound >= 0) dasics_libcfg_free(observation_bound);
    if (allowed_one_bound >= 0) dasics_libcfg_free(allowed_one_bound);
    if (allowed_two_bound >= 0) dasics_libcfg_free(allowed_two_bound);
    if (gap_low_bound >= 0) dasics_libcfg_free(gap_low_bound);
    if (gap_high_bound >= 0) dasics_libcfg_free(gap_high_bound);
    unregister_udasics();
#if DASICS_LINUX_DUAL_EXEC
    printf(DASICS_APP_SYSCALL_TAG
           " summary scope=linux-exec-mode dasics=%s total=%lu failed=%d result=%s\n",
           dasics_linux_exec_mode_name(mode), total, failures,
           failures ? "FAIL" : "PASS");
#else
    printf(DASICS_APP_SYSCALL_TAG
           " summary scope=os-controlled-syscall-pairs total=%lu failed=%d result=%s\n",
           DASICS_APP_SYSCALL_CASES, failures,
           failures ? "FAIL" : "PASS");
#endif
    return failures ? 1 : 0;
}
