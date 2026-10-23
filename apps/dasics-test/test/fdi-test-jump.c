#include "fdi_demo.h"

volatile struct fdi_demo_observation fdi_jump_observed ATTR_ULIB_DATA;
extern int fdi_jump_probe(void);
extern char fdi_jump_freezone_entry[], fdi_jump_freezone_end[];
extern char fdi_jump_library_return[], fdi_jump_site[], fdi_jump_recovery[], fdi_jump_target[];

asm(
    ".option push\n.option norvc\n.option norelax\n"
    ".pushsection .ulibtext,\"ax\",@progbits\n.balign 8\n"
    ".global fdi_jump_probe\n.type fdi_jump_probe,@function\nfdi_jump_probe:\n"
    "mv t6,ra\nli a2,0\nli a3,0\nla t5,fdi_jump_library_return\n"
    "jal ra,fdi_jump_freezone_entry\n"
    ".global fdi_jump_library_return\nfdi_jump_library_return:\n"
    "la t1,fdi_jump_observed\nsd a2,16(t1)\nsd a3,24(t1)\nsd t6,32(t1)\n"
    "mv ra,t6\nli a0,0\nret\n.size fdi_jump_probe,.-fdi_jump_probe\n.popsection\n"
    ".pushsection .ufreezonetext,\"ax\",@progbits\n.balign 8\n"
    ".global fdi_jump_freezone_entry\n.type fdi_jump_freezone_entry,@function\n"
    "fdi_jump_freezone_entry:\naddi a3,a3,1\nla t0,fdi_jump_target\n"
    "li t1,0\nli t2,0xd15ea5ed5a17c0de\nli t3,0\n"
    ".global fdi_jump_site\nfdi_jump_site:\njalr ra,0(t0)\n"
    ".global fdi_jump_recovery\nfdi_jump_recovery:\njalr zero,0(t5)\n"
    ".global fdi_jump_freezone_end\nfdi_jump_freezone_end:\n"
    ".size fdi_jump_freezone_entry,.-fdi_jump_freezone_entry\n.popsection\n"
    ".pushsection .text,\"ax\",@progbits\n.balign 8\n"
    ".global fdi_jump_target\n.type fdi_jump_target,@function\nfdi_jump_target:\n"
    "addi a2,a2,1\njalr zero,0(t5)\n.size fdi_jump_target,.-fdi_jump_target\n"
    ".popsection\n.option pop\n"
);

int main(int argc, char **argv)
{
    struct fdi_demo demo;
    if (fdi_demo_begin(&demo, argc, argv, "jump", 0))
        return 2;
    if (!fdi_demo_setup(&demo, &fdi_jump_observed, NULL, 0,
                          fdi_jump_freezone_entry, fdi_jump_freezone_end))
        return fdi_demo_finish(&demo, -1);
    csr_write(CSR_DFZRETURN, (uintptr_t)fdi_jump_library_return);
    demo.source = demo.address = demo.target = (uintptr_t)fdi_jump_target;
    demo.site = (uintptr_t)fdi_jump_site;
    demo.recovery = (uintptr_t)fdi_jump_recovery;
    demo.reason = EXC_DASICS_JUMP_FAULT;
    demo.fault_ra = (uintptr_t)fdi_jump_library_return;
    demo.fault_t2 = FDI_DEMO_POISON;
    int result = fdi_demo_call(&demo, fdi_jump_probe);
    fdi_demo_observe(&demo);
    fdi_demo_check(&demo, "TARGET-HITS", fdi_jump_observed.hits,
                   demo.mode == DASICS_LINUX_EXEC_OFF);
    fdi_demo_check(&demo, "FREEZONE-PATH", fdi_jump_observed.path, 1);
    return fdi_demo_finish(&demo, result);
}
