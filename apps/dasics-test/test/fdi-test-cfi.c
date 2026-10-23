#include "fdi_demo.h"

volatile struct fdi_demo_observation fdi_cfi_observed ATTR_ULIB_DATA;
extern int fdi_cfi_probe(void);
extern char fdi_cfi_freezone_entry[], fdi_cfi_freezone_end[];
extern char fdi_cfi_library_return[], fdi_cfi_site[], fdi_cfi_recovery[], fdi_cfi_target[];

/* The vulnerable frame has a real saved return address at sp+8. Overwriting
 * and reloading that slot makes the attack independent of C stack layouts. */
asm(
    ".option push\n.option norvc\n.option norelax\n"
    ".pushsection .ulibtext,\"ax\",@progbits\n.balign 8\n"
    ".global fdi_cfi_probe\n.type fdi_cfi_probe,@function\nfdi_cfi_probe:\n"
    "mv t6,ra\nli a2,0\nli a3,0\nla t5,fdi_cfi_library_return\n"
    "jal ra,fdi_cfi_freezone_entry\n"
    ".global fdi_cfi_library_return\nfdi_cfi_library_return:\n"
    "la t1,fdi_cfi_observed\nsd a2,16(t1)\nsd a3,24(t1)\nsd t6,32(t1)\n"
    "sd a5,56(t1)\nsd a4,64(t1)\nsd a6,72(t1)\nsd sp,80(t1)\n"
    "mv ra,t6\nli a0,0\nret\n.size fdi_cfi_probe,.-fdi_cfi_probe\n.popsection\n"
    ".pushsection .ufreezonetext,\"ax\",@progbits\n.balign 8\n"
    ".global fdi_cfi_freezone_entry\n.type fdi_cfi_freezone_entry,@function\n"
    "fdi_cfi_freezone_entry:\naddi a3,a3,1\nmv a6,sp\naddi sp,sp,-16\n"
    "sd ra,8(sp)\nmv a5,ra\nla t0,fdi_cfi_target\n"
    ".global fdi_cfi_overwrite\nfdi_cfi_overwrite:\nsd t0,8(sp)\n"
    ".global fdi_cfi_reload\nfdi_cfi_reload:\nld ra,8(sp)\nmv a4,ra\n"
    "li t1,0\nli t2,0xd15ea5ed5a17c0de\nli t3,0\n"
    ".global fdi_cfi_site\nfdi_cfi_site:\njalr zero,0(ra)\n"
    ".global fdi_cfi_recovery\nfdi_cfi_recovery:\naddi sp,sp,16\njalr zero,0(t5)\n"
    ".global fdi_cfi_freezone_end\nfdi_cfi_freezone_end:\n"
    ".size fdi_cfi_freezone_entry,.-fdi_cfi_freezone_entry\n.popsection\n"
    ".pushsection .text,\"ax\",@progbits\n.balign 8\n"
    ".global fdi_cfi_target\n.type fdi_cfi_target,@function\nfdi_cfi_target:\n"
    "addi a2,a2,1\nj fdi_cfi_recovery\n.size fdi_cfi_target,.-fdi_cfi_target\n"
    ".popsection\n.option pop\n"
);

int main(int argc, char **argv)
{
    struct fdi_demo demo;
    if (fdi_demo_begin(&demo, argc, argv, "cfi", 0))
        return 2;
    if (!fdi_demo_setup(&demo, &fdi_cfi_observed, NULL, 0,
                          fdi_cfi_freezone_entry, fdi_cfi_freezone_end))
        return fdi_demo_finish(&demo, -1);
    csr_write(CSR_DFZRETURN, (uintptr_t)fdi_cfi_library_return);
    demo.source = demo.address = demo.target = (uintptr_t)fdi_cfi_target;
    demo.site = (uintptr_t)fdi_cfi_site;
    demo.recovery = (uintptr_t)fdi_cfi_recovery;
    demo.reason = EXC_DASICS_JUMP_FAULT;
    demo.fault_ra = (uintptr_t)fdi_cfi_target;
    demo.fault_t2 = FDI_DEMO_POISON;
    int result = fdi_demo_call(&demo, fdi_cfi_probe);
    fdi_demo_observe(&demo);
    fdi_demo_check(&demo, "TARGET-HITS", fdi_cfi_observed.hits,
                   demo.mode == DASICS_LINUX_EXEC_OFF);
    fdi_demo_check(&demo, "FREEZONE-PATH", fdi_cfi_observed.path, 1);
    fdi_demo_check(&demo, "ORIGINAL-RA", fdi_cfi_observed.original_ra,
                   (uintptr_t)fdi_cfi_library_return);
    fdi_demo_check(&demo, "SAVED-RA", fdi_cfi_observed.saved_ra, (uintptr_t)fdi_cfi_target);
    fdi_demo_check(&demo, "STACK-RESTORED", fdi_cfi_observed.sp_before != 0 &&
                   fdi_cfi_observed.sp_before == fdi_cfi_observed.sp_after, 1);
    return fdi_demo_finish(&demo, result);
}
