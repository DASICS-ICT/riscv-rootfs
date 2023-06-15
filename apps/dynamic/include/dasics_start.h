#ifndef _DASICS_START_H_
#define _DASICS_START_H_
#include <asm/dasics_ucontext.h>
typedef void (*rtld_fini) (void);
void _dasics_entry_stage1(uint64_t sp);
void _dasics_entry_stage2(uint64_t sp, rtld_fini fini);
void _dasics_entry_stage3(uint64_t sp, rtld_fini fini);

void _dasics_start_ufault_entry(regs_context_t *regs);
extern void __dasics_start_ufault_entry();
extern void _umain_entry();
extern void _copy_lib_entry();

#endif