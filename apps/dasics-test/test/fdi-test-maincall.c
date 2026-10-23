#include "fdi_demo.h"

volatile struct fdi_demo_observation fdi_maincall_observed ATTR_ULIB_DATA;
const char fdi_maincall_message[] = FDI_DEMO_TAG " service demo=maincall message=trusted-print\n";
extern int fdi_maincall_probe(void);
extern char fdi_maincall_site[], fdi_maincall_recovery[];

/* Count only a real request from the library entry and use the maintained
 * trusted printing service. Its saved ReturnPC and active return must survive. */
uint64_t fdi_maincall_service(UmaincallTypes type, ...)
{
    va_list args;
    va_start(args, type);
    const char *format = va_arg(args, const char *);
    va_end(args);
    if (type != Umaincall_PRINT || format != fdi_maincall_message)
        return ~0UL;
    struct fdi_demo_state before = fdi_demo_read_state();
    fdi_maincall_observed.service_count++;
    uint64_t result = dasics_umaincall_helper(type, format);
    struct fdi_demo_state after = fdi_demo_read_state();
    fdi_maincall_observed.bound_preserved = !memcmp(&before, &after, sizeof(before));
    return result;
}

asm(
    ".pushsection .ulibtext,\"ax\",@progbits\n.option push\n.option norvc\n.option norelax\n"
    ".balign 8\n.global fdi_maincall_probe\n.type fdi_maincall_probe,@function\n"
    "fdi_maincall_probe:\naddi sp,sp,-16\nsd ra,8(sp)\n"
    "li a0,0\nla a1,fdi_maincall_message\n"
    ".global fdi_maincall_site\nfdi_maincall_site:\njal ra,dasics_umaincall\n"
    ".global fdi_maincall_recovery\nfdi_maincall_recovery:\n"
    "la t1,fdi_maincall_observed\nsd a0,0(t1)\nli t0,1\nsd t0,24(t1)\n"
    "ld ra,8(sp)\nsd ra,32(t1)\naddi sp,sp,16\nli a0,0\nret\n"
    ".size fdi_maincall_probe,.-fdi_maincall_probe\n.option pop\n.popsection\n"
);

int main(int argc, char **argv)
{
    struct fdi_demo demo;
    if (fdi_demo_begin(&demo, argc, argv, "maincall", 0))
        return 2;
    if (!fdi_demo_setup(&demo, &fdi_maincall_observed, NULL, 0, NULL, NULL))
        return fdi_demo_finish(&demo, -1);
    register_udasics((uintptr_t)fdi_maincall_service);
    demo.site = (uintptr_t)fdi_maincall_site;
    demo.recovery = (uintptr_t)fdi_maincall_recovery;
    demo.target = (uintptr_t)dasics_umaincall;
    int result = fdi_demo_call(&demo, fdi_maincall_probe);
    fdi_demo_observe(&demo);
    fdi_demo_check(&demo, "SERVICE-COUNT", fdi_maincall_observed.service_count, 1);
    fdi_demo_check(&demo, "SERVICE-RETURN", fdi_maincall_observed.value, 0);
    fdi_demo_check(&demo, "SERVICE-PATH", fdi_maincall_observed.path, 1);
    fdi_demo_check(&demo, "BOUND-PRESERVED", fdi_maincall_observed.bound_preserved, 1);
    return fdi_demo_finish(&demo, result);
}
