#include "fdi_demo.h"

/* Separate aligned objects prevent the read grant from including either guard. */
asm(
    ".pushsection .ulibdata,\"aw\",@progbits\n.balign 8\n"
    ".global fdi_finegrain_before\n.type fdi_finegrain_before,@object\n"
    "fdi_finegrain_before:\n.fill 8,1,0x5a\n.size fdi_finegrain_before,8\n"
    ".global fdi_finegrain_buffer\n.type fdi_finegrain_buffer,@object\n"
    "fdi_finegrain_buffer:\n.fill 8,1,0x61\n.size fdi_finegrain_buffer,8\n"
    ".global fdi_finegrain_after\n.type fdi_finegrain_after,@object\n"
    "fdi_finegrain_after:\n.fill 8,1,0xa5\n.size fdi_finegrain_after,8\n.popsection\n"
);
extern unsigned char fdi_finegrain_before[8], fdi_finegrain_buffer[8], fdi_finegrain_after[8];
volatile struct fdi_demo_observation fdi_finegrain_observed ATTR_ULIB_DATA;
extern int fdi_finegrain_probe(void);
extern char fdi_finegrain_site[], fdi_finegrain_recovery[];

asm(
    ".pushsection .ulibtext,\"ax\",@progbits\n.option push\n.option norvc\n.option norelax\n"
    ".balign 8\n.global fdi_finegrain_probe\n.type fdi_finegrain_probe,@function\n"
    "fdi_finegrain_probe:\nmv t6,ra\nla t0,fdi_finegrain_buffer\nlbu t3,0(t0)\n"
    "li t1,66\nli t2,0\n.global fdi_finegrain_site\nfdi_finegrain_site:\nsb t1,7(t0)\n"
    ".global fdi_finegrain_recovery\nfdi_finegrain_recovery:\n"
    "la t1,fdi_finegrain_observed\nsd t3,8(t1)\nsd t6,32(t1)\nli a0,0\nret\n"
    ".size fdi_finegrain_probe,.-fdi_finegrain_probe\n.option pop\n.popsection\n"
);

int main(int argc, char **argv)
{
    struct fdi_demo demo;
    if (fdi_demo_begin(&demo, argc, argv, "finegrain", 0))
        return 2;
    unsigned char original[8], expected[8];
    memcpy(original, fdi_finegrain_buffer, sizeof(original));
    memcpy(expected, original, sizeof(expected));
    if (!fdi_demo_setup(&demo, &fdi_finegrain_observed, fdi_finegrain_buffer, 8, NULL, NULL))
        return fdi_demo_finish(&demo, -1);
    demo.source = (uintptr_t)fdi_finegrain_buffer;
    demo.address = demo.source + 7;
    demo.site = (uintptr_t)fdi_finegrain_site;
    demo.recovery = (uintptr_t)fdi_finegrain_recovery;
    demo.reason = EXC_DASICS_STORE_FAULT;
    demo.fault_ra = demo.mode == DASICS_LINUX_EXEC_ON ?
        (uintptr_t)lib_call + 12 : (uintptr_t)fdi_demo_off_return;
    demo.fault_t1 = 66;
    demo.fault_t3 = 97;
    int result = fdi_demo_call(&demo, fdi_finegrain_probe);
    if (demo.mode == DASICS_LINUX_EXEC_OFF)
        expected[7] = 66;
    fdi_finegrain_observed.value = fdi_finegrain_buffer[7];
    fdi_finegrain_observed.unchanged = !memcmp(expected, fdi_finegrain_buffer, 8);
    int guards = 1;
    for (unsigned int i = 0; i < 8; i++)
        guards &= fdi_finegrain_before[i] == 0x5a && fdi_finegrain_after[i] == 0xa5;
    fdi_demo_observe(&demo);
    fdi_demo_check(&demo, "READ-VALUE", fdi_finegrain_observed.read_value, 97);
    fdi_demo_check(&demo, "VALUE", fdi_finegrain_observed.value, expected[7]);
    fdi_demo_check(&demo, "GUARDS", guards, 1);
    fdi_demo_check(&demo, "BUFFERS", fdi_finegrain_observed.unchanged, 1);
    memcpy(fdi_finegrain_buffer, original, sizeof(original));
    return fdi_demo_finish(&demo, result);
}
