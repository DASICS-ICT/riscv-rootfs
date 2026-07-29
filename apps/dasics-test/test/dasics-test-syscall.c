#include <stdint.h>
#include <stdio.h>
#include <machine/syscall.h>

#include "udasics.h"

#define DASICS_APP_SYSCALL_TAG "[DASICS-APP-SYSCALL]"
#define DASICS_APP_SYSCALL_CASES 11UL
#define SYSCALL_OUTPUT_LEN 8UL
#define SYSCALL_GAP_LEN 24UL
#define SYSCALL_STDOUT_FD 1L

#ifdef DASICS_N_EXTENSION_PROFILE
#define SYSCALL_DENIED_RETURN (-1L)
#else
#define SYSCALL_DENIED_RETURN SYSCALL_STDOUT_FD
#endif

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
#ifdef DASICS_N_EXTENSION_PROFILE
    register long a0 asm("a0") = SYSCALL_STDOUT_FD;
    register long a1 asm("a1") = (long)attack_output;
    register long a2 asm("a2") = SYSCALL_OUTPUT_LEN;
    register long a7 asm("a7") = SYS_write;

    asm volatile(
        ".global dasics_n_extension_syscall_attack_ecall\n"
        "dasics_n_extension_syscall_attack_ecall:\n"
        "ecall"
        : "+r"(a0)
        : "r"(a1), "r"(a2), "r"(a7)
        : "memory");
    syscall_observations.attack_return = a0;
#else
    syscall_observations.attack_return =
        ulib_write(SYSCALL_STDOUT_FD, attack_output, SYSCALL_OUTPUT_LEN);
#endif
    return 0;
}

int ATTR_ULIB_TEXT syscall_allowed_one_operation(void)
{
#ifdef DASICS_N_EXTENSION_PROFILE
    register long a0 asm("a0") = SYSCALL_STDOUT_FD;
    register long a1 asm("a1") = (long)allowed_output_one;
    register long a2 asm("a2") = SYSCALL_OUTPUT_LEN;
    register long a7 asm("a7") = SYS_write;

    asm volatile(
        ".global dasics_n_extension_syscall_allowed_one_ecall\n"
        "dasics_n_extension_syscall_allowed_one_ecall:\n"
        "ecall"
        : "+r"(a0)
        : "r"(a1), "r"(a2), "r"(a7)
        : "memory");
    syscall_observations.allowed_one_return = a0;
#else
    syscall_observations.allowed_one_return =
        ulib_write(SYSCALL_STDOUT_FD, allowed_output_one,
                   SYSCALL_OUTPUT_LEN);
#endif
    return 0;
}

int ATTR_ULIB_TEXT syscall_allowed_two_operation(void)
{
#ifdef DASICS_N_EXTENSION_PROFILE
    register long a0 asm("a0") = SYSCALL_STDOUT_FD;
    register long a1 asm("a1") = (long)allowed_output_two;
    register long a2 asm("a2") = SYSCALL_OUTPUT_LEN;
    register long a7 asm("a7") = SYS_write;

    asm volatile(
        ".global dasics_n_extension_syscall_allowed_two_ecall\n"
        "dasics_n_extension_syscall_allowed_two_ecall:\n"
        "ecall"
        : "+r"(a0)
        : "r"(a1), "r"(a2), "r"(a7)
        : "memory");
    syscall_observations.allowed_two_return = a0;
#else
    syscall_observations.allowed_two_return =
        ulib_write(SYSCALL_STDOUT_FD, allowed_output_two,
                   SYSCALL_OUTPUT_LEN);
#endif
    return 0;
}

int ATTR_ULIB_TEXT syscall_gap_operation(void)
{
#ifdef DASICS_N_EXTENSION_PROFILE
    register long a0 asm("a0") = SYSCALL_STDOUT_FD;
    register long a1 asm("a1") = (long)gap_output;
    register long a2 asm("a2") = SYSCALL_GAP_LEN;
    register long a7 asm("a7") = SYS_write;

    asm volatile(
        ".global dasics_n_extension_syscall_gap_ecall\n"
        "dasics_n_extension_syscall_gap_ecall:\n"
        "ecall"
        : "+r"(a0)
        : "r"(a1), "r"(a2), "r"(a7)
        : "memory");
    syscall_observations.gap_return = a0;
#else
    syscall_observations.gap_return =
        ulib_write(SYSCALL_STDOUT_FD, gap_output, SYSCALL_GAP_LEN);
#endif
    return 0;
}
#pragma GCC pop_options

#ifdef DASICS_N_EXTENSION_PROFILE
extern char dasics_n_extension_syscall_attack_ecall[];
extern char dasics_n_extension_syscall_allowed_one_ecall[];
extern char dasics_n_extension_syscall_allowed_two_ecall[];
extern char dasics_n_extension_syscall_gap_ecall[];

static int check_n_extension_syscall_records(void)
{
    static const char *const names[DASICS_N_EXTENSION_SYSCALL_RECORDS] = {
        "unbounded-deny", "bounded-one-allow", "bounded-two-allow",
        "gapped-deny",
    };
    static const uint64_t expected_permitted[
        DASICS_N_EXTENSION_SYSCALL_RECORDS] = {0, 1, 1, 0};
    static const int64_t expected_result[
        DASICS_N_EXTENSION_SYSCALL_RECORDS] = {
            SYSCALL_DENIED_RETURN, SYSCALL_OUTPUT_LEN, SYSCALL_OUTPUT_LEN,
            SYSCALL_DENIED_RETURN,
        };
    const uint64_t expected_uepc[DASICS_N_EXTENSION_SYSCALL_RECORDS] = {
        (uint64_t)dasics_n_extension_syscall_attack_ecall,
        (uint64_t)dasics_n_extension_syscall_allowed_one_ecall,
        (uint64_t)dasics_n_extension_syscall_allowed_two_ecall,
        (uint64_t)dasics_n_extension_syscall_gap_ecall,
    };
    int failures = 0;

    for (uint64_t i = 0; i < DASICS_N_EXTENSION_SYSCALL_RECORDS; i++) {
        const volatile dasics_n_extension_syscall_record_t *record =
            &dasics_n_extension_syscall_records[i];
        int pass =
            record->ucause == 0x18 &&
            record->ustatus == 0x10 &&
            record->uepc == expected_uepc[i] &&
            record->utval == 0 &&
            record->dfreason == EXC_DASICS_ECALL_FAULT &&
            record->permitted == expected_permitted[i] &&
            record->result == expected_result[i];

        printf(
            "DASICS_N_EXTENSION_SYSCALL_TRAP version=1 sequence=%lu "
            "case=%s ucause=0x%lx ustatus=0x%lx uepc=0x%lx "
            "utval=0x%lx dfreason=0x%lx permitted=%lu result=0x%lx "
            "expect_uepc=0x%lx check=%s\n",
            i + 1, names[i], record->ucause, record->ustatus,
            record->uepc, record->utval, record->dfreason,
            record->permitted, (uint64_t)record->result, expected_uepc[i],
            pass ? "PASS" : "FAIL");
        failures += !pass;
    }

    if (dasics_n_extension_syscall_trap_count !=
            DASICS_N_EXTENSION_SYSCALL_RECORDS ||
        dasics_n_extension_syscall_permitted_count != 2 ||
        dasics_n_extension_syscall_denied_count != 2) {
        failures++;
    }
    printf(
        "DASICS_N_EXTENSION_SYSCALL_PROFILE version=1 traps=%lu "
        "permitted=%lu denied=%lu failed=%d result=%s\n",
        dasics_n_extension_syscall_trap_count,
        dasics_n_extension_syscall_permitted_count,
        dasics_n_extension_syscall_denied_count, failures,
        failures ? "FAIL" : "PASS");
    return failures;
}
#endif

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

int main(void)
{
    uint64_t attack_reason = 0;
    uint64_t gap_reason = 0;
    long pid = dasics_complete_app_getpid();
    int failures = 0;

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
#ifdef DASICS_N_EXTENSION_PROFILE
    /*
     * Make the HU entry/uret UIE->UPIE transition observable, as in the OFB
     * N profile.  Each returned proxy call restores UIE before the next fault.
     */
    csr_write(CSR_USTATUS, 1);
#endif
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
                                  SYSCALL_DENIED_RETURN);
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
                                  SYSCALL_DENIED_RETURN);

#ifdef DASICS_N_EXTENSION_PROFILE
    failures += check_n_extension_syscall_records();
#endif

    long restored = dasics_complete_app_control(
        DASICS_COMPLETE_APP_CONTROL_RESTORE, (uint64_t)pid,
        DASICS_COMPLETE_APP_STAGE_RESTORE, DASICS_COMPLETE_APP_CFG_UENA);
    failures += record_value_case("SYSCALL-OS-CFG-RESTORED",
                                  (uint64_t)restored,
                                  DASICS_COMPLETE_APP_CFG_UENA);

    if (observation_bound >= 0) dasics_libcfg_free(observation_bound);
    if (allowed_one_bound >= 0) dasics_libcfg_free(allowed_one_bound);
    if (allowed_two_bound >= 0) dasics_libcfg_free(allowed_two_bound);
    if (gap_low_bound >= 0) dasics_libcfg_free(gap_low_bound);
    if (gap_high_bound >= 0) dasics_libcfg_free(gap_high_bound);
    unregister_udasics();
    printf(DASICS_APP_SYSCALL_TAG
           " summary scope=os-controlled-syscall-pairs total=%lu failed=%d result=%s\n",
           DASICS_APP_SYSCALL_CASES, failures,
           failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
