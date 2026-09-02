#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "udasics.h"

#ifndef DASICS_N_EXTENSION_PROFILE
#error "the interactive protection demo requires DASICS_N_EXTENSION_PROFILE"
#endif

#define DEMO_INITIAL_VALUE 0x1122334455667788UL
#define DEMO_ATTEMPT_VALUE 0xaabbccddeeff0011UL

static volatile uint64_t dasics_interactive_target
    __attribute__((aligned(8))) = DEMO_INITIAL_VALUE;

extern char dasics_interactive_store_fault[];
extern char dasics_interactive_store_recovery[];
extern void dasics_interactive_untrusted_store(void);

asm(
    ".option push\n"
    ".option norvc\n"
    ".option norelax\n"
    ".section .ulibtext,\"ax\",@progbits\n"
    ".balign 8\n"
    ".global dasics_interactive_untrusted_store\n"
    ".type dasics_interactive_untrusted_store, @function\n"
    "dasics_interactive_untrusted_store:\n"
    "  la t0, dasics_interactive_target\n"
    "  li t1, 0xaabbccddeeff0011\n"
    ".global dasics_interactive_store_fault\n"
    "dasics_interactive_store_fault:\n"
    "  sd t1, 0(t0)\n"
    ".global dasics_interactive_store_recovery\n"
    "dasics_interactive_store_recovery:\n"
    "  ret\n"
    ".size dasics_interactive_untrusted_store, "
    ".-dasics_interactive_untrusted_store\n"
    ".section .text,\"ax\",@progbits\n"
    ".option pop\n");

static void invoke_untrusted_store(void)
{
    asm volatile(
        "la a0, dasics_interactive_untrusted_store\n"
        ".word 0x0005108b\n"
        :
        :
        : "ra", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
          "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");
}

static int protected_record_is_exact(uint64_t before, uint64_t after)
{
    volatile dasics_n_extension_trap_record_t *record;

    if (before >= DASICS_N_EXTENSION_TRAP_RECORDS || after != before + 1)
        return 0;

    record = &dasics_n_extension_trap_records[before];
    return record->ucause == DASICS_N_EXTENSION_UCHECK_CAUSE &&
           record->dfreason == EXC_DASICS_STORE_FAULT &&
           record->uepc == (uint64_t)(uintptr_t)dasics_interactive_store_fault &&
           record->utval == (uint64_t)(uintptr_t)&dasics_interactive_target &&
           record->recovery ==
               (uint64_t)(uintptr_t)dasics_interactive_store_recovery;
}

int main(int argc, char **argv)
{
    int protected_mode;
    long pid;
    long requested_cfg;
    uint64_t observed_cfg;
    uint64_t traps_before;
    uint64_t traps_after;
    uint64_t final_value;
    int passed;

    if (argc == 1) {
        protected_mode = 0;
    } else if (argc == 2 && !strcmp(argv[1], "-dasics")) {
        protected_mode = 1;
    } else {
        printf("usage: %s [-dasics]\n", argv[0]);
        return 2;
    }

    dasics_interactive_target = DEMO_INITIAL_VALUE;
    csr_write(CSR_DLCFG0, 0);
    csr_write(CSR_DFREASON, 0);
    csr_write(CSR_USCRATCH, 0);
    pid = dasics_complete_app_getpid();
    if (pid <= 0) {
        printf("DASICS_INTERACTIVE_DEMO_SETUP result=FAIL reason=getpid "
               "value=%ld\n", pid);
        return 1;
    }
    if (protected_mode) {
        requested_cfg = DASICS_COMPLETE_APP_CFG_UENA;
    } else {
        requested_cfg = dasics_complete_app_control(
            DASICS_COMPLETE_APP_CONTROL_SET, (uint64_t)pid,
            DASICS_COMPLETE_APP_STAGE_A, DASICS_COMPLETE_APP_CFG_OFF);
    }
    observed_cfg = (uint64_t)dasics_complete_app_control(
        DASICS_COMPLETE_APP_CONTROL_QUERY, (uint64_t)pid, 0, 0);
    if (requested_cfg !=
            (protected_mode ? DASICS_COMPLETE_APP_CFG_UENA
                            : DASICS_COMPLETE_APP_CFG_OFF) ||
        observed_cfg != (uint64_t)requested_cfg) {
        printf("DASICS_INTERACTIVE_DEMO_SETUP result=FAIL "
               "requested=0x%lx observed=0x%lx\n",
               (uint64_t)requested_cfg, observed_cfg);
        return 1;
    }

    printf("DASICS_INTERACTIVE_DEMO_BEGIN version=1 mode=%s "
           "argument=%s\n",
           protected_mode ? "protected" : "unprotected",
           protected_mode ? "-dasics" : "none");
    printf("DASICS_INTERACTIVE_DEMO_CONFIG protection=%s "
           "umaincfg=0x%lx libcfg=0x%lx\n",
           protected_mode ? "ON" : "OFF", observed_cfg,
           csr_read(CSR_DLCFG0));
    printf("DASICS_INTERACTIVE_DEMO_ACCESS operation=store "
           "address=0x%lx before=0x%lx attempt=0x%lx\n",
           (uint64_t)(uintptr_t)&dasics_interactive_target,
           (uint64_t)dasics_interactive_target, DEMO_ATTEMPT_VALUE);

    traps_before = dasics_n_extension_trap_count;
    if (protected_mode)
        csr_write(CSR_USCRATCH,
                  (uint64_t)(uintptr_t)dasics_interactive_store_recovery);
    if (protected_mode)
        invoke_untrusted_store();
    else
        dasics_interactive_untrusted_store();
    csr_write(CSR_USCRATCH, 0);
    traps_after = dasics_n_extension_trap_count;
    final_value = dasics_interactive_target;

    if (protected_mode) {
        passed = final_value == DEMO_INITIAL_VALUE &&
                 protected_record_is_exact(traps_before, traps_after);
        dasics_n_extension_dump_context();
    } else {
        passed = final_value == DEMO_ATTEMPT_VALUE &&
                 traps_after == traps_before;
    }

    printf("DASICS_INTERACTIVE_DEMO_RESULT version=1 mode=%s "
           "protection=%s access=%s before=0x%lx after=0x%lx "
           "traps=%lu result=%s\n",
           protected_mode ? "protected" : "unprotected",
           protected_mode ? "ON" : "OFF",
           protected_mode ? "BLOCKED" : "ALLOWED", DEMO_INITIAL_VALUE,
           final_value, traps_after - traps_before,
           passed ? "PASS" : "FAIL");
    return passed ? 0 : 1;
}
