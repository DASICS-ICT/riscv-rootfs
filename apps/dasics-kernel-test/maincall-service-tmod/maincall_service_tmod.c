// SPDX-License-Identifier: GPL-2.0-only

#include <linux/dasics.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>

#include "../maincall-service-utmod/maincall_service_utmod.h"

static struct dasics_call_frame maincall_service_frame;
static struct dasics_compartment maincall_service_compartment;
static u8 maincall_service_stack_probe[1024] __aligned(16);

static long maincall_service_run(void *target, unsigned long arg1,
		   struct dasics_call_regs *regs)
{
	struct dasics_call_policy policy = {
		.callee = &maincall_service_compartment,
		.target = target,
		.stack_size = 512,
	};

	memset(&maincall_service_frame, 0, sizeof(maincall_service_frame));
	memset(regs, 0, sizeof(*regs));
	regs->a0 = (unsigned long)dasics_maincall_gate;
	regs->a1 = arg1;
	return dasics_call(&maincall_service_frame, &policy, regs);
}

static int __init maincall_service_tmod_init(void)
{
	struct dasics_call_regs regs;
	unsigned int i;
	long ret;

	ret = dasics_compartment_init_module(&maincall_service_compartment,
					     maincall_service_valid_service);
	if (ret)
		goto fail;

	ret = maincall_service_run(maincall_service_valid_service, 0, &regs);
	if (ret || regs.ret_a0 ||
	    regs.ret_a1 != DASICS_MAINCALL_ABI_VERSION)
		goto fail;
	pr_info("[DASICS-MAINCALL-SERVICE]: PASS legal read-only service returned ABI version %lu\n",
		regs.ret_a1);

	ret = maincall_service_run(maincall_service_full_registers, 0, &regs);
	if (ret || regs.ret_a0 ||
	    regs.ret_a1 != DASICS_MAINCALL_REGISTER_TEST_VALUE)
		goto fail;
	pr_info("[DASICS-MAINCALL-SERVICE]: PASS a0-a7 request registers preserved\n");

	ret = maincall_service_run(maincall_service_unknown_service, 0, &regs);
	if (ret || (long)regs.ret_a0 != -ENOSYS || regs.ret_a1)
		goto fail;
	pr_info("[DASICS-MAINCALL-SERVICE]: PASS unknown service ID rejected\n");

	ret = maincall_service_run(maincall_service_bad_arguments, 0, &regs);
	if (ret || (long)regs.ret_a0 != -EINVAL || regs.ret_a1)
		goto fail;
	pr_info("[DASICS-MAINCALL-SERVICE]: PASS malformed service arguments rejected\n");

	ret = maincall_service_run(maincall_service_invalid_source, 0, &regs);
	if (ret != -EPERM || maincall_service_frame.state != DASICS_CALL_FRAME_CLEANED ||
	    maincall_service_frame.parent_saved || maincall_service_frame.preempt_held)
		goto fail;
	pr_info("[DASICS-MAINCALL-SERVICE]: PASS invalid source unwound to outer continuation\n");

	memset(maincall_service_stack_probe, 0x5a, sizeof(maincall_service_stack_probe));
	ret = maincall_service_run(maincall_service_untrusted_stack,
		     (unsigned long)&maincall_service_stack_probe[768], &regs);
	if (ret || regs.ret_a0 ||
	    regs.ret_a1 != DASICS_MAINCALL_ABI_VERSION)
		goto fail;
	for (i = 0; i < ARRAY_SIZE(maincall_service_stack_probe); i++) {
		if (maincall_service_stack_probe[i] != 0x5a)
			goto fail;
	}
	pr_info("[DASICS-MAINCALL-SERVICE]: PASS maincall gate ignored untrusted-supplied stack\n");
	pr_info("[DASICS-MAINCALL-SERVICE]: PASS fixed maincall ABI and whitelist dispatch tests\n");
	return 0;

fail:
	dasics_call_finish(&maincall_service_frame);
	dasics_compartment_destroy(&maincall_service_compartment);
	pr_err("[DASICS-MAINCALL-SERVICE]: FAIL ret=%ld maincall_service_status=%ld value=%lu state=%u saved=%u preempt=%u\n",
	       ret, (long)regs.ret_a0, regs.ret_a1, maincall_service_frame.state,
	       maincall_service_frame.parent_saved, maincall_service_frame.preempt_held);
	return ret ?: -EINVAL;
}

static void __exit maincall_service_tmod_exit(void)
{
	dasics_compartment_destroy(&maincall_service_compartment);
}

module_init(maincall_service_tmod_init);
module_exit(maincall_service_tmod_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DASICS maincall service maincall ABI and whitelist tests");
