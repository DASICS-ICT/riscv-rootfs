#include "fdi_demo.h"

unsigned char fdi_flexibility_buffers[100][64] ATTR_ULIB_DATA;
volatile struct fdi_demo_observation fdi_flexibility_observed ATTR_ULIB_DATA;

#define FDI_FLEX_FUNCTIONS(M) \
    M(0) M(1) M(2) M(3) M(4) M(5) M(6) M(7) M(8) M(9) \
    M(10) M(11) M(12) M(13) M(14) M(15) M(16) M(17) M(18) M(19) \
    M(20) M(21) M(22) M(23) M(24) M(25) M(26) M(27) M(28) M(29) \
    M(30) M(31) M(32) M(33) M(34) M(35) M(36) M(37) M(38) M(39) \
    M(40) M(41) M(42) M(43) M(44) M(45) M(46) M(47) M(48) M(49) \
    M(50) M(51) M(52) M(53) M(54) M(55) M(56) M(57) M(58) M(59) \
    M(60) M(61) M(62) M(63) M(64) M(65) M(66) M(67) M(68) M(69) \
    M(70) M(71) M(72) M(73) M(74) M(75) M(76) M(77) M(78) M(79) \
    M(80) M(81) M(82) M(83) M(84) M(85) M(86) M(87) M(88) M(89) \
    M(90) M(91) M(92) M(93) M(94) M(95) M(96) M(97) M(98) M(99)

#define FDI_FLEX_DECLARE(n) \
    extern int fdi_flexibility_probe_##n(void); \
    extern char fdi_flexibility_site_##n[], fdi_flexibility_recovery_##n[];
FDI_FLEX_FUNCTIONS(FDI_FLEX_DECLARE)
#undef FDI_FLEX_DECLARE

/* Each function encodes its own row and identity. Selection affects only the
 * function pointer and the one read grant, never the probe's data address. */
#define FDI_FLEX_ASM(n) \
    ".balign 8\n.global fdi_flexibility_probe_" #n "\n" \
    ".type fdi_flexibility_probe_" #n ",@function\nfdi_flexibility_probe_" #n ":\n" \
    "mv t6,ra\nla t0,fdi_flexibility_buffers\nli t4,(" #n "*64)\nadd t0,t0,t4\n" \
    "lbu t3,0(t0)\nli t1,238\nli t2,0\n" \
    ".global fdi_flexibility_site_" #n "\nfdi_flexibility_site_" #n ":\nsb t1,32(t0)\n" \
    ".global fdi_flexibility_recovery_" #n "\nfdi_flexibility_recovery_" #n ":\n" \
    "la t1,fdi_flexibility_observed\nsd t3,8(t1)\nsd t6,32(t1)\n" \
    "li t4," #n "\nsd t4,40(t1)\nli a0,0\nret\n" \
    ".size fdi_flexibility_probe_" #n ",.-fdi_flexibility_probe_" #n "\n"

asm(
    ".pushsection .ulibtext,\"ax\",@progbits\n.option push\n.option norvc\n.option norelax\n"
    FDI_FLEX_FUNCTIONS(FDI_FLEX_ASM)
    ".option pop\n.popsection\n"
);
#undef FDI_FLEX_ASM

static int (*const functions[100])(void) = {
#define FDI_FLEX_ENTRY(n) fdi_flexibility_probe_##n,
    FDI_FLEX_FUNCTIONS(FDI_FLEX_ENTRY)
#undef FDI_FLEX_ENTRY
};
static char *const sites[100] = {
#define FDI_FLEX_SITE(n) fdi_flexibility_site_##n,
    FDI_FLEX_FUNCTIONS(FDI_FLEX_SITE)
#undef FDI_FLEX_SITE
};
static char *const recoveries[100] = {
#define FDI_FLEX_RECOVERY(n) fdi_flexibility_recovery_##n,
    FDI_FLEX_FUNCTIONS(FDI_FLEX_RECOVERY)
#undef FDI_FLEX_RECOVERY
};

int main(int argc, char **argv)
{
    struct fdi_demo demo;
    if (fdi_demo_begin(&demo, argc, argv, "flexibility", 1))
        return 2;
    for (unsigned int i = 0; i < 100; i++)
        memset(fdi_flexibility_buffers[i], i + 1, 64);
    unsigned int selected = demo.function;
    if (!fdi_demo_setup(&demo, &fdi_flexibility_observed,
                          fdi_flexibility_buffers[selected], 64, NULL, NULL))
        return fdi_demo_finish(&demo, -1);
    demo.source = (uintptr_t)fdi_flexibility_buffers[selected];
    demo.address = demo.source + 32;
    demo.site = (uintptr_t)sites[selected];
    demo.recovery = (uintptr_t)recoveries[selected];
    demo.reason = EXC_DASICS_STORE_FAULT;
    demo.fault_ra = demo.mode == DASICS_LINUX_EXEC_ON ?
        (uintptr_t)lib_call + 12 : (uintptr_t)fdi_demo_off_return;
    demo.fault_t1 = 238;
    demo.fault_t3 = selected + 1;
    int result = fdi_demo_call(&demo, functions[selected]);
    int buffers = 1;
    for (unsigned int i = 0; i < 100; i++) {
        for (unsigned int j = 0; j < 64; j++) {
            unsigned char expected = i + 1;
            if (demo.mode == DASICS_LINUX_EXEC_OFF && i == selected && j == 32)
                expected = 238;
            buffers &= fdi_flexibility_buffers[i][j] == expected;
        }
    }
    fdi_flexibility_observed.value = fdi_flexibility_buffers[selected][32];
    fdi_flexibility_observed.unchanged = buffers;
    fdi_demo_observe(&demo);
    fdi_demo_check(&demo, "FUNCTION-ID", fdi_flexibility_observed.selected, selected);
    fdi_demo_check(&demo, "READ-VALUE", fdi_flexibility_observed.read_value, selected + 1);
    fdi_demo_check(&demo, "VALUE", fdi_flexibility_observed.value,
        demo.mode == DASICS_LINUX_EXEC_OFF ? 238 : selected + 1);
    fdi_demo_check(&demo, "BUFFERS", buffers, 1);
    memset(fdi_flexibility_buffers, 0, sizeof(fdi_flexibility_buffers));
    return fdi_demo_finish(&demo, result);
}
