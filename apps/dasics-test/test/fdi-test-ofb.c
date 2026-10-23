#include "fdi_demo.h"

unsigned char fdi_ofb_secret[8] ATTR_ULIB_DATA = "Secret!";
volatile struct fdi_demo_observation fdi_ofb_observed ATTR_ULIB_DATA;
extern int fdi_ofb_probe(void);
extern char fdi_ofb_site[], fdi_ofb_recovery[];

asm(
    ".pushsection .ulibtext,\"ax\",@progbits\n.option push\n.option norvc\n.option norelax\n"
    ".balign 8\n.global fdi_ofb_probe\n.type fdi_ofb_probe,@function\nfdi_ofb_probe:\n"
    "mv t6,ra\nla t0,fdi_ofb_secret\nli t1,0\nli t2,0xd15ea5ed5a17c0de\nli t3,0\n"
    ".global fdi_ofb_site\nfdi_ofb_site:\nlbu t2,0(t0)\n"
    ".global fdi_ofb_recovery\nfdi_ofb_recovery:\n"
    "la t1,fdi_ofb_observed\nsd t2,0(t1)\nsd t6,32(t1)\nli a0,0\nret\n"
    ".size fdi_ofb_probe,.-fdi_ofb_probe\n.option pop\n.popsection\n"
);

int main(int argc, char **argv)
{
    struct fdi_demo demo;
    if (fdi_demo_begin(&demo, argc, argv, "ofb", 0))
        return 2;
    unsigned char original[sizeof(fdi_ofb_secret)];
    memcpy(original, fdi_ofb_secret, sizeof(original));
    if (!fdi_demo_setup(&demo, &fdi_ofb_observed, NULL, 0, NULL, NULL))
        return fdi_demo_finish(&demo, -1);
    demo.source = demo.address = (uintptr_t)fdi_ofb_secret;
    demo.site = (uintptr_t)fdi_ofb_site;
    demo.recovery = (uintptr_t)fdi_ofb_recovery;
    demo.reason = EXC_DASICS_LOAD_FAULT;
    demo.fault_ra = demo.mode == DASICS_LINUX_EXEC_ON ?
        (uintptr_t)lib_call + 12 : (uintptr_t)fdi_demo_off_return;
    demo.fault_t2 = FDI_DEMO_POISON;
    int result = fdi_demo_call(&demo, fdi_ofb_probe);
    fdi_ofb_observed.unchanged = !memcmp(original, fdi_ofb_secret, sizeof(original));
    fdi_demo_observe(&demo);
    fdi_demo_check(&demo, "VALUE", fdi_ofb_observed.value,
        demo.mode == DASICS_LINUX_EXEC_ON ? FDI_DEMO_POISON : 83);
    fdi_demo_check(&demo, "BUFFERS", fdi_ofb_observed.unchanged, 1);
    memcpy(fdi_ofb_secret, original, sizeof(original));
    return fdi_demo_finish(&demo, result);
}
