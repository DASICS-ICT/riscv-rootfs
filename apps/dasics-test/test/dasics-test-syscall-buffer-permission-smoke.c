#include <stdint.h>
#include <stdio.h>

#include "ucsr.h"

#define DASICS_SYSCALL_BUFFER_TAG "\033[1;34m[DASICS-SYSCALL-BUFFER-PERMISSION]\033[0m"
#define DASICS_SYSCALL_BUFFER_BEGIN_TAG "\033[1;31m[DASICS-SYSCALL-BUFFER-PERMISSION]"
#define DASICS_SYSCALL_BUFFER_SUMMARY_TAG "\033[1;32m[DASICS-SYSCALL-BUFFER-PERMISSION]"
#define DASICS_SYSCALL_BUFFER_COLOR_END "\033[0m"

#define STR_IMPL(value) #value
#define XSTR(value) STR_IMPL(value)

#define SYS_READ 63UL
#define SYS_WRITE 64UL
#define SYS_PREAD 67UL
#define SYS_PWRITE 68UL
#define STDOUT_FD 1UL

#define DASICS_LIB_CFG_WRITE 0x1UL
#define DASICS_LIB_CFG_READ 0x2UL
#define DASICS_LIB_CFG_VALID 0x8UL
#define DASICS_LIB_CFG_MASK 0xfUL
#define DASICS_LIB_CFG_SLOT_BITS 4UL
#define DASICS_LIB_ENTRY_NUM 16
#define DASICS_FREASON_ECALL 1UL

#define BUFFER_LEN 16UL
#define SPLIT_BOUND_LEN 8UL
#define PARTIAL_BUFFER_LEN 24UL
#define PARTIAL_TAIL_OFFSET 16UL
#define PARTIAL_TAIL_LEN 8UL
#define REAL_WRITE_LEN 46UL
#define REAL_WRITE_BOUND_LEN 48UL
#define UNTRUSTED_WRITE_LEN 16UL

static volatile unsigned char write_readable_buffer[BUFFER_LEN]
    __attribute__((aligned(8))) = "write-readable";
static volatile unsigned char write_split_buffer[BUFFER_LEN]
    __attribute__((aligned(8))) = "write-split-ok";
static volatile unsigned char write_partial_buffer[PARTIAL_BUFFER_LEN]
    __attribute__((aligned(8))) = "write-gap-deny";
static volatile unsigned char read_writable_buffer[BUFFER_LEN]
    __attribute__((aligned(8)));
static volatile unsigned char pwrite_readable_buffer[BUFFER_LEN]
    __attribute__((aligned(8))) = "pwrite-allowed";
static volatile unsigned char pread_writable_buffer[BUFFER_LEN]
    __attribute__((aligned(8)));
static const char syscall_buffer_real_write_message[REAL_WRITE_LEN + 1]
    __attribute__((aligned(8))) =
    "[DASICS-SYSCALL-BUFFER] trusted write allowed\n";
static volatile char syscall_buffer_untrusted_write_message[UNTRUSTED_WRITE_LEN + 1]
    __attribute__((used, aligned(8))) = "untrusted write!";

extern char syscall_buffer_untrusted_write_entry[];

asm (
".option push\n"
".option norvc\n"
".option norelax\n"
".section .ulibtext,\"ax\",@progbits\n"
".balign 8\n"
".global syscall_buffer_untrusted_write_entry\n"
".type syscall_buffer_untrusted_write_entry, @function\n"
"syscall_buffer_untrusted_write_entry:\n"
"  li a0, " XSTR(STDOUT_FD) "\n"
"  la a1, syscall_buffer_untrusted_write_message\n"
"  li a2, " XSTR(UNTRUSTED_WRITE_LEN) "\n"
"  li a7, " XSTR(SYS_WRITE) "\n"
"  ecall\n"
"  ret\n"
".section .text,\"ax\",@progbits\n"
".option pop\n"
);

static unsigned long invoke_untrusted_write_ecall(void)
{
    unsigned long result;

    asm volatile (
        "addi sp, sp, -16\n"
        "sd ra, 8(sp)\n"
        "la a0, syscall_buffer_untrusted_write_entry\n"
        ".word 0x0005108b\n"
        "mv %0, a0\n"
        "ld ra, 8(sp)\n"
        "addi sp, sp, 16\n"
        : "=r"(result)
        :
        : "ra", "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7",
          "t0", "t1", "t2", "t3", "t4", "t5", "t6", "memory");

    return result;
}

static long trusted_sys_write(const char *buffer, unsigned long len)
{
    register unsigned long a0 asm("a0") = STDOUT_FD;
    register unsigned long a1 asm("a1") = (unsigned long)buffer;
    register unsigned long a2 asm("a2") = len;
    register unsigned long a7 asm("a7") = SYS_WRITE;

    asm volatile (
        "ecall"
        : "+r"(a0)
        : "r"(a1), "r"(a2), "r"(a7)
        : "memory");

    return (long)a0;
}

static unsigned long lib_cfg_slot(unsigned int index, unsigned long cfg)
{
    return cfg << (index * DASICS_LIB_CFG_SLOT_BITS);
}

static unsigned long read_lib_cfg_slot(unsigned int index)
{
    return (csr_read(0x880) >> (index * DASICS_LIB_CFG_SLOT_BITS)) &
           DASICS_LIB_CFG_MASK;
}

#define READ_BOUND_LO_CASE(index) \
    case index: \
        return csr_read(0x890 + (index) * 2)

#define READ_BOUND_HI_CASE(index) \
    case index: \
        return csr_read(0x891 + (index) * 2)

#define WRITE_BOUND_CASE(index) \
    case index: \
        csr_write(0x890 + (index) * 2, lo); \
        csr_write(0x891 + (index) * 2, hi); \
        break

static unsigned long read_lib_bound_lo(unsigned int index)
{
    switch (index) {
    READ_BOUND_LO_CASE(0);
    READ_BOUND_LO_CASE(1);
    READ_BOUND_LO_CASE(2);
    READ_BOUND_LO_CASE(3);
    READ_BOUND_LO_CASE(4);
    READ_BOUND_LO_CASE(5);
    READ_BOUND_LO_CASE(6);
    READ_BOUND_LO_CASE(7);
    READ_BOUND_LO_CASE(8);
    READ_BOUND_LO_CASE(9);
    READ_BOUND_LO_CASE(10);
    READ_BOUND_LO_CASE(11);
    READ_BOUND_LO_CASE(12);
    READ_BOUND_LO_CASE(13);
    READ_BOUND_LO_CASE(14);
    READ_BOUND_LO_CASE(15);
    default:
        return 0;
    }
}

static unsigned long read_lib_bound_hi(unsigned int index)
{
    switch (index) {
    READ_BOUND_HI_CASE(0);
    READ_BOUND_HI_CASE(1);
    READ_BOUND_HI_CASE(2);
    READ_BOUND_HI_CASE(3);
    READ_BOUND_HI_CASE(4);
    READ_BOUND_HI_CASE(5);
    READ_BOUND_HI_CASE(6);
    READ_BOUND_HI_CASE(7);
    READ_BOUND_HI_CASE(8);
    READ_BOUND_HI_CASE(9);
    READ_BOUND_HI_CASE(10);
    READ_BOUND_HI_CASE(11);
    READ_BOUND_HI_CASE(12);
    READ_BOUND_HI_CASE(13);
    READ_BOUND_HI_CASE(14);
    READ_BOUND_HI_CASE(15);
    default:
        return 0;
    }
}

static void write_lib_bound_slot(unsigned int index, const volatile void *base,
                                 unsigned long len)
{
    unsigned long lo = (unsigned long)base;
    unsigned long hi = lo + len;

    switch (index) {
    WRITE_BOUND_CASE(0);
    WRITE_BOUND_CASE(1);
    WRITE_BOUND_CASE(2);
    WRITE_BOUND_CASE(3);
    WRITE_BOUND_CASE(4);
    WRITE_BOUND_CASE(5);
    WRITE_BOUND_CASE(6);
    WRITE_BOUND_CASE(7);
    WRITE_BOUND_CASE(8);
    WRITE_BOUND_CASE(9);
    WRITE_BOUND_CASE(10);
    WRITE_BOUND_CASE(11);
    WRITE_BOUND_CASE(12);
    WRITE_BOUND_CASE(13);
    WRITE_BOUND_CASE(14);
    WRITE_BOUND_CASE(15);
    default:
        break;
    }
}

static void install_lib_bound(unsigned int index, unsigned long cfg,
                              const volatile void *base, unsigned long len)
{
    unsigned long libcfg = csr_read(0x880);

    write_lib_bound_slot(index, base, len);
    libcfg &= ~lib_cfg_slot(index, DASICS_LIB_CFG_MASK);
    libcfg |= lib_cfg_slot(index, cfg | DASICS_LIB_CFG_VALID);
    csr_write(0x880, libcfg);
}

static void clear_syscall_buffer_csrs(void)
{
    csr_write(0x880, 0);
    csr_write(0x8b3, 0);
}

static int buffer_has_required_bound(const volatile void *buffer,
                                     unsigned long len,
                                     unsigned long required_cfg)
{
    unsigned long start = (unsigned long)buffer;
    unsigned long end = start + len;
    unsigned long cursor = start;

    if (len == 0) {
        return 1;
    }

    if (end < start) {
        return 0;
    }

    while (cursor < end) {
        unsigned long best = cursor;

        for (unsigned int i = 0; i < DASICS_LIB_ENTRY_NUM; i++) {
            unsigned long cfg = read_lib_cfg_slot(i);
            unsigned long lo;
            unsigned long hi;

            if ((cfg & (required_cfg | DASICS_LIB_CFG_VALID)) !=
                (required_cfg | DASICS_LIB_CFG_VALID)) {
                continue;
            }

            lo = read_lib_bound_lo(i);
            hi = read_lib_bound_hi(i);
            if (lo <= cursor && cursor < hi && hi > best) {
                best = hi;
            }
        }

        if (best == cursor) {
            return 0;
        }
        cursor = best;
    }

    return 1;
}

static int syscall_buffer_permitted(unsigned long sysno,
                                    const volatile void *buffer,
                                    unsigned long len)
{
    switch (sysno) {
    case SYS_READ:
    case SYS_PREAD:
        return buffer_has_required_bound(buffer, len, DASICS_LIB_CFG_WRITE);
    case SYS_WRITE:
    case SYS_PWRITE:
        return buffer_has_required_bound(buffer, len, DASICS_LIB_CFG_READ);
    default:
        return 1;
    }
}

static int record_bool_case(const char *case_name, int value, int expect,
                            unsigned long *total)
{
    int pass = value == expect;

    (*total)++;
    printf(DASICS_SYSCALL_BUFFER_TAG " case=%s value=0x%x expect=0x%x result=%s\n",
           case_name, value, expect, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

static int record_long_case(const char *case_name, long value, long expect,
                            unsigned long *total)
{
    int pass = value == expect;

    (*total)++;
    printf(DASICS_SYSCALL_BUFFER_TAG " case=%s value=0x%lx expect=0x%lx result=%s\n",
           case_name, (unsigned long)value, (unsigned long)expect,
           pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

static int record_freason_case(const char *case_name, unsigned long expect,
                               unsigned long *total)
{
    unsigned long freason = csr_read(0x8b3);
    int pass = freason == expect;

    (*total)++;
    printf(DASICS_SYSCALL_BUFFER_TAG " case=%s freason=0x%lx expect=0x%lx result=%s\n",
           case_name, freason, expect, pass ? "PASS" : "FAIL");

    return pass ? 0 : 1;
}

int main(void)
{
    unsigned long total = 0;
    int failures = 0;
    int allowed;
    long written;

    printf(DASICS_SYSCALL_BUFFER_BEGIN_TAG " syscall buffer permission smoke begin" DASICS_SYSCALL_BUFFER_COLOR_END "\n");

    clear_syscall_buffer_csrs();
    install_lib_bound(0, DASICS_LIB_CFG_READ, write_readable_buffer, BUFFER_LEN);
    allowed = syscall_buffer_permitted(SYS_WRITE, write_readable_buffer,
                                       BUFFER_LEN);
    failures += record_bool_case("SYSCALL-BUFFER-WRITE-READABLE-BOUND-ALLOW",
                                 allowed, 1, &total);

    clear_syscall_buffer_csrs();
    allowed = syscall_buffer_permitted(SYS_WRITE, write_readable_buffer,
                                       BUFFER_LEN);
    failures += record_bool_case("SYSCALL-BUFFER-WRITE-UNBOUNDED-DENY",
                                 allowed, 0, &total);

    clear_syscall_buffer_csrs();
    install_lib_bound(0, DASICS_LIB_CFG_READ, write_split_buffer,
                      SPLIT_BOUND_LEN);
    install_lib_bound(1, DASICS_LIB_CFG_READ,
                      write_split_buffer + SPLIT_BOUND_LEN, SPLIT_BOUND_LEN);
    allowed = syscall_buffer_permitted(SYS_WRITE, write_split_buffer,
                                       BUFFER_LEN);
    failures += record_bool_case("SYSCALL-BUFFER-WRITE-SPLIT-BOUND-ALLOW",
                                 allowed, 1, &total);

    clear_syscall_buffer_csrs();
    install_lib_bound(0, DASICS_LIB_CFG_READ, write_partial_buffer,
                      SPLIT_BOUND_LEN);
    install_lib_bound(1, DASICS_LIB_CFG_READ,
                      write_partial_buffer + PARTIAL_TAIL_OFFSET,
                      PARTIAL_TAIL_LEN);
    allowed = syscall_buffer_permitted(SYS_WRITE, write_partial_buffer,
                                       PARTIAL_BUFFER_LEN);
    failures += record_bool_case("SYSCALL-BUFFER-WRITE-PARTIAL-BOUND-DENY",
                                 allowed, 0, &total);

    clear_syscall_buffer_csrs();
    install_lib_bound(0, DASICS_LIB_CFG_WRITE, read_writable_buffer,
                      BUFFER_LEN);
    allowed = syscall_buffer_permitted(SYS_READ, read_writable_buffer,
                                       BUFFER_LEN);
    failures += record_bool_case("SYSCALL-BUFFER-READ-WRITABLE-BOUND-ALLOW",
                                 allowed, 1, &total);

    clear_syscall_buffer_csrs();
    install_lib_bound(0, DASICS_LIB_CFG_READ, read_writable_buffer, BUFFER_LEN);
    allowed = syscall_buffer_permitted(SYS_READ, read_writable_buffer,
                                       BUFFER_LEN);
    failures += record_bool_case("SYSCALL-BUFFER-READ-READONLY-BOUND-DENY",
                                 allowed, 0, &total);

    clear_syscall_buffer_csrs();
    install_lib_bound(0, DASICS_LIB_CFG_READ, pwrite_readable_buffer,
                      BUFFER_LEN);
    allowed = syscall_buffer_permitted(SYS_PWRITE, pwrite_readable_buffer,
                                       BUFFER_LEN);
    failures += record_bool_case("SYSCALL-BUFFER-PWRITE-READABLE-BOUND-ALLOW",
                                 allowed, 1, &total);

    clear_syscall_buffer_csrs();
    install_lib_bound(0, DASICS_LIB_CFG_WRITE, pread_writable_buffer,
                      BUFFER_LEN);
    allowed = syscall_buffer_permitted(SYS_PREAD, pread_writable_buffer,
                                       BUFFER_LEN);
    failures += record_bool_case("SYSCALL-BUFFER-PREAD-WRITABLE-BOUND-ALLOW",
                                 allowed, 1, &total);

    clear_syscall_buffer_csrs();
    allowed = syscall_buffer_permitted(SYS_WRITE, write_readable_buffer, 0);
    failures += record_bool_case("SYSCALL-BUFFER-ZERO-LENGTH-ALLOW",
                                 allowed, 1, &total);

    clear_syscall_buffer_csrs();
    install_lib_bound(0, DASICS_LIB_CFG_READ,
                      syscall_buffer_real_write_message,
                      REAL_WRITE_BOUND_LEN);
    allowed = syscall_buffer_permitted(SYS_WRITE,
                                       syscall_buffer_real_write_message,
                                       REAL_WRITE_LEN);
    failures += record_bool_case("SYSCALL-BUFFER-REAL-WRITE-CHECKER-ALLOW",
                                 allowed, 1, &total);
    written = trusted_sys_write(syscall_buffer_real_write_message,
                                REAL_WRITE_LEN);
    failures += record_long_case("SYSCALL-BUFFER-REAL-WRITE-TRUSTED-ALLOW",
                                 written, REAL_WRITE_LEN, &total);

    clear_syscall_buffer_csrs();
    (void)invoke_untrusted_write_ecall();
    failures += record_freason_case("SYSCALL-BUFFER-UNTRUSTED-ECALL-FREASON",
                                    DASICS_FREASON_ECALL, &total);

    printf(DASICS_SYSCALL_BUFFER_SUMMARY_TAG " summary total=%lu failed=%d result=%s" DASICS_SYSCALL_BUFFER_COLOR_END "\n",
           total, failures, failures ? "FAIL" : "PASS");

    return failures ? 1 : 0;
}
