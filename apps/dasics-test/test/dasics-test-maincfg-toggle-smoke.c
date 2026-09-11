#include <stdint.h>
#include <stdio.h>
#ifdef DASICS_N_EXTENSION_PROFILE
#include "udasics.h"
#endif

#define STR_IMPL(value) #value
#define XSTR(value) STR_IMPL(value)

#define MAINCFG_TOGGLE_MAGIC 0x4644494d43464755UL
#define MAINCFG_TOGGLE_ARM 0x41524dUL
#define MAINCFG_TOGGLE_REPORT 0x525054UL
#define MAINCFG_TOGGLE_HU_REPORT 0x4855525054UL
#define MAINCFG_TOGGLE_GETPID 306UL
#define MAINCFG_TOGGLE_OBSERVATIONS 16UL

#define MAINCFG_TOGGLE_ECALL_SENTINEL 0x4543414c4c534954UL
#define MAINCFG_TOGGLE_ECALL_GUARD 0x47554152444d4346UL
#define MAINCFG_TOGGLE_LOAD_RD_SENTINEL 0x5a5a5a5a5a5a5a5aUL
#define MAINCFG_TOGGLE_LOAD_VALUE 0x554c4f4144444154UL
#define MAINCFG_TOGGLE_STORE_ATTEMPT 0x5553544f52454421UL
#define MAINCFG_TOGGLE_JUMP_LINK_SENTINEL 0x1122334455667788UL
#define MAINCFG_TOGGLE_JUMP_INITIAL 0x554a554d50494e49UL
#define MAINCFG_TOGGLE_JUMP_TAKEN 0x554a554d5054414bUL

enum maincfg_toggle_operation {
    MAINCFG_TOGGLE_ECALL,
    MAINCFG_TOGGLE_LOAD,
    MAINCFG_TOGGLE_STORE,
    MAINCFG_TOGGLE_JUMP,
};

extern char maincfg_toggle_ecall_site[];
extern char maincfg_toggle_load_site[];
extern char maincfg_toggle_store_site[];
extern char maincfg_toggle_jump_site[];
extern char maincfg_toggle_ecall_recovery[];
extern char maincfg_toggle_load_recovery[];
extern char maincfg_toggle_store_recovery[];
extern char maincfg_toggle_jump_recovery[];
extern char maincfg_toggle_jump_target[];

extern uint64_t maincfg_toggle_ecall_invoke(void);
extern uint64_t maincfg_toggle_load_invoke(const volatile uint64_t *address);
extern uint64_t maincfg_toggle_store_invoke(volatile uint64_t *address,
                                            uint64_t value);
extern void maincfg_toggle_jump_invoke(uint64_t target, uint64_t *link,
                                       uint64_t *marker);

static volatile uint64_t maincfg_toggle_ecall_guard = MAINCFG_TOGGLE_ECALL_GUARD;
static volatile uint64_t maincfg_toggle_load_data = MAINCFG_TOGGLE_LOAD_VALUE;
static volatile uint64_t maincfg_toggle_store_data;
static char *const maincfg_toggle_sites[] = {
    maincfg_toggle_ecall_site,
    maincfg_toggle_load_site,
    maincfg_toggle_store_site,
    maincfg_toggle_jump_site,
};
static char *const maincfg_toggle_recoveries[] = {
    maincfg_toggle_ecall_recovery,
    maincfg_toggle_load_recovery,
    maincfg_toggle_store_recovery,
    maincfg_toggle_jump_recovery,
};
static const volatile uint64_t *const maincfg_toggle_operands[] = {
    &maincfg_toggle_ecall_guard,
    &maincfg_toggle_load_data,
    &maincfg_toggle_store_data,
    (const volatile uint64_t *)maincfg_toggle_jump_target,
};

asm(
    ".option push\n"
    ".option norvc\n"
    ".section .text,\"ax\",@progbits\n"
    ".balign 8\n"
    ".global maincfg_toggle_ecall_invoke\n"
    ".type maincfg_toggle_ecall_invoke, @function\n"
    "maincfg_toggle_ecall_invoke:\n"
    "  addi sp, sp, -16\n"
    "  sd ra, 8(sp)\n"
    "  li a0, " XSTR(MAINCFG_TOGGLE_ECALL_SENTINEL) "\n"
    "  li a7, " XSTR(MAINCFG_TOGGLE_GETPID) "\n"
    "  la t0, maincfg_toggle_ecall_source\n"
    ".global maincfg_toggle_ecall_entry_jalr\n"
    "maincfg_toggle_ecall_entry_jalr:\n"
    "  jalr ra, 0(t0)\n"
    ".global maincfg_toggle_ecall_recovery\n"
    "maincfg_toggle_ecall_recovery:\n"
    "  ld ra, 8(sp)\n"
    "  addi sp, sp, 16\n"
    "  ret\n"
    ".size maincfg_toggle_ecall_invoke, .-maincfg_toggle_ecall_invoke\n"

    ".balign 8\n"
    ".global maincfg_toggle_load_invoke\n"
    ".type maincfg_toggle_load_invoke, @function\n"
    "maincfg_toggle_load_invoke:\n"
    "  addi sp, sp, -16\n"
    "  sd ra, 8(sp)\n"
    "  li t2, " XSTR(MAINCFG_TOGGLE_LOAD_RD_SENTINEL) "\n"
    "  la t0, maincfg_toggle_load_source\n"
    ".global maincfg_toggle_load_entry_jalr\n"
    "maincfg_toggle_load_entry_jalr:\n"
    "  jalr ra, 0(t0)\n"
    ".global maincfg_toggle_load_recovery\n"
    "maincfg_toggle_load_recovery:\n"
    "  mv a0, t2\n"
    "  ld ra, 8(sp)\n"
    "  addi sp, sp, 16\n"
    "  ret\n"
    ".size maincfg_toggle_load_invoke, .-maincfg_toggle_load_invoke\n"

    ".balign 8\n"
    ".global maincfg_toggle_store_invoke\n"
    ".type maincfg_toggle_store_invoke, @function\n"
    "maincfg_toggle_store_invoke:\n"
    "  addi sp, sp, -16\n"
    "  sd ra, 8(sp)\n"
    "  la t0, maincfg_toggle_store_source\n"
    ".global maincfg_toggle_store_entry_jalr\n"
    "maincfg_toggle_store_entry_jalr:\n"
    "  jalr ra, 0(t0)\n"
    ".global maincfg_toggle_store_recovery\n"
    "maincfg_toggle_store_recovery:\n"
    "  mv a0, a1\n"
    "  ld ra, 8(sp)\n"
    "  addi sp, sp, 16\n"
    "  ret\n"
    ".size maincfg_toggle_store_invoke, .-maincfg_toggle_store_invoke\n"

    ".balign 8\n"
    ".global maincfg_toggle_jump_invoke\n"
    ".type maincfg_toggle_jump_invoke, @function\n"
    "maincfg_toggle_jump_invoke:\n"
    "  addi sp, sp, -32\n"
    "  sd ra, 24(sp)\n"
    "  sd a1, 16(sp)\n"
    "  sd a2, 8(sp)\n"
    "  li t2, " XSTR(MAINCFG_TOGGLE_JUMP_LINK_SENTINEL) "\n"
    "  li t3, " XSTR(MAINCFG_TOGGLE_JUMP_INITIAL) "\n"
    "  la t0, maincfg_toggle_jump_source\n"
    ".global maincfg_toggle_jump_entry_jalr\n"
    "maincfg_toggle_jump_entry_jalr:\n"
    "  jalr ra, 0(t0)\n"
    ".global maincfg_toggle_jump_recovery\n"
    "maincfg_toggle_jump_recovery:\n"
    "  ld a1, 16(sp)\n"
    "  ld a2, 8(sp)\n"
    "  sd t2, 0(a1)\n"
    "  sd t3, 0(a2)\n"
    "  ld ra, 24(sp)\n"
    "  addi sp, sp, 32\n"
    "  ret\n"
    ".size maincfg_toggle_jump_invoke, .-maincfg_toggle_jump_invoke\n"

    ".balign 8\n"
    ".global maincfg_toggle_jump_target\n"
    ".type maincfg_toggle_jump_target, @function\n"
    "maincfg_toggle_jump_target:\n"
    "  li t3, " XSTR(MAINCFG_TOGGLE_JUMP_TAKEN) "\n"
    ".global maincfg_toggle_jump_target_return\n"
    "maincfg_toggle_jump_target_return:\n"
    "  jr t2\n"
    ".size maincfg_toggle_jump_target, .-maincfg_toggle_jump_target\n"

    ".section .ulibtext,\"ax\",@progbits\n"
    ".balign 8\n"
    ".global maincfg_toggle_ecall_source\n"
    "maincfg_toggle_ecall_source:\n"
    ".global maincfg_toggle_ecall_site\n"
    "maincfg_toggle_ecall_site:\n"
    "  ecall\n"
    ".global maincfg_toggle_ecall_return_site\n"
    "maincfg_toggle_ecall_return_site:\n"
    "  jr ra\n"

    ".balign 8\n"
    ".global maincfg_toggle_load_source\n"
    "maincfg_toggle_load_source:\n"
    ".global maincfg_toggle_load_site\n"
    "maincfg_toggle_load_site:\n"
    "  ld t2, 0(a0)\n"
    ".global maincfg_toggle_load_return_site\n"
    "maincfg_toggle_load_return_site:\n"
    "  jr ra\n"

    ".balign 8\n"
    ".global maincfg_toggle_store_source\n"
    "maincfg_toggle_store_source:\n"
    ".global maincfg_toggle_store_site\n"
    "maincfg_toggle_store_site:\n"
    "  sd a1, 0(a0)\n"
    ".global maincfg_toggle_store_return_site\n"
    "maincfg_toggle_store_return_site:\n"
    "  jr ra\n"

    ".balign 8\n"
    ".global maincfg_toggle_jump_source\n"
    "maincfg_toggle_jump_source:\n"
    ".global maincfg_toggle_jump_site\n"
    "maincfg_toggle_jump_site:\n"
    "  jalr t2, 0(a0)\n"
    ".global maincfg_toggle_jump_return_site\n"
    "maincfg_toggle_jump_return_site:\n"
    "  jr ra\n"
    ".section .text,\"ax\",@progbits\n"
    ".option pop\n");

static long maincfg_toggle_getpid(void)
{
    register uint64_t a0 asm("a0") = 0;
    register uint64_t a7 asm("a7") = MAINCFG_TOGGLE_GETPID;

    asm volatile("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return (long)a0;
}

static long maincfg_toggle_control(uint64_t command, uint64_t pid,
                                   uint64_t step, uint64_t arg4,
                                   uint64_t arg5, uint64_t arg6)
{
    register uint64_t a0 asm("a0") = MAINCFG_TOGGLE_MAGIC;
    register uint64_t a1 asm("a1") = command;
    register uint64_t a2 asm("a2") = pid;
    register uint64_t a3 asm("a3") = step;
    register uint64_t a4 asm("a4") = arg4;
    register uint64_t a5 asm("a5") = arg5;
    register uint64_t a6 asm("a6") = arg6;
    register uint64_t a7 asm("a7") = MAINCFG_TOGGLE_GETPID;

    asm volatile("ecall"
                 : "+r"(a0)
                 : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
                   "r"(a6), "r"(a7)
                 : "memory");
    return (long)a0;
}

static uint64_t symbol_address(char *symbol)
{
    return (uint64_t)(uintptr_t)symbol;
}

static int arm_observation(uint64_t pid, uint64_t step,
                           enum maincfg_toggle_operation op)
{
    return maincfg_toggle_control(
               MAINCFG_TOGGLE_ARM, pid, step,
               symbol_address(maincfg_toggle_sites[op]),
               (uint64_t)(uintptr_t)maincfg_toggle_operands[op],
               symbol_address(maincfg_toggle_recoveries[op])) == (long)pid
               ? 0
               : 1;
}

#ifdef DASICS_N_EXTENSION_PROFILE
static int report_hu_fault(uint64_t pid, uint64_t step,
                           uint64_t trap_before)
{
    uint64_t trap_after = dasics_n_extension_trap_count;
    uint64_t packed_cause_reason;
    volatile dasics_n_extension_trap_record_t *record;

    csr_write(CSR_USCRATCH, 0);
    if (trap_after == trap_before) {
        return 0;
    }
    if (trap_before >= DASICS_N_EXTENSION_TRAP_RECORDS ||
        trap_after != trap_before + 1) {
        return 1;
    }

    record = &dasics_n_extension_trap_records[trap_before];
    packed_cause_reason =
        (record->ucause << 32) | (record->dfreason & 0xffffffffUL);
    return maincfg_toggle_control(
               MAINCFG_TOGGLE_HU_REPORT, pid, step, record->uepc,
               record->utval, packed_cause_reason) == (long)pid
               ? 0
               : 1;
}
#endif

static int report_observation(uint64_t pid, uint64_t step, uint64_t side0,
                              uint64_t side1)
{
    return maincfg_toggle_control(MAINCFG_TOGGLE_REPORT, pid, step, side0,
                                  side1, 0) == (long)pid
               ? 0
               : 1;
}

int main(void)
{
    uint64_t step = 0;
    long pid = maincfg_toggle_getpid();
    int failures = 0;

    printf("DASICS_MAINCFG_TOGGLE_BEGIN version=1 observations=16\n");
    if (pid <= 0) {
        printf("DASICS_MAINCFG_TOGGLE_CLIENT result=FAIL detail=getpid\n");
        return 1;
    }

    for (int op = MAINCFG_TOGGLE_ECALL; op <= MAINCFG_TOGGLE_JUMP; op++) {
        for (int stage = 0; stage < 4; stage++, step++) {
            uint64_t side0 = 0;
            uint64_t side1 = 0;
#ifdef DASICS_N_EXTENSION_PROFILE
            uint64_t trap_before = dasics_n_extension_trap_count;
#endif

            failures += arm_observation((uint64_t)pid, step, op);
#ifdef DASICS_N_EXTENSION_PROFILE
            csr_write(CSR_USCRATCH,
                      symbol_address(maincfg_toggle_recoveries[op]));
#endif
            switch (op) {
            case MAINCFG_TOGGLE_ECALL:
                side0 = maincfg_toggle_ecall_invoke();
                side1 = maincfg_toggle_ecall_guard;
                break;
            case MAINCFG_TOGGLE_LOAD:
                side0 = maincfg_toggle_load_invoke(&maincfg_toggle_load_data);
                side1 = maincfg_toggle_load_data;
                break;
            case MAINCFG_TOGGLE_STORE:
                side1 = maincfg_toggle_store_invoke(
                    &maincfg_toggle_store_data, MAINCFG_TOGGLE_STORE_ATTEMPT);
                side0 = maincfg_toggle_store_data;
                break;
            case MAINCFG_TOGGLE_JUMP:
                side0 = MAINCFG_TOGGLE_JUMP_LINK_SENTINEL;
                side1 = MAINCFG_TOGGLE_JUMP_INITIAL;
                maincfg_toggle_jump_invoke(
                    symbol_address(maincfg_toggle_jump_target), &side0, &side1);
                break;
            }
#ifdef DASICS_N_EXTENSION_PROFILE
            failures += report_hu_fault((uint64_t)pid, step, trap_before);
#endif
            failures += report_observation((uint64_t)pid, step, side0, side1);
        }
    }

    if (step != MAINCFG_TOGGLE_OBSERVATIONS || failures != 0) {
        printf("DASICS_MAINCFG_TOGGLE_CLIENT result=FAIL step=%lu failed=%d\n",
               step, failures);
        return 1;
    }
    return 0;
}
