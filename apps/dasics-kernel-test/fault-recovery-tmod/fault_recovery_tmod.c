// SPDX-License-Identifier: GPL-2.0-only

#include <linux/dasics.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>

#include "../fault-recovery-utmod/fault_recovery_utmod.h"

static struct dasics_call_frame fault_recovery_frame;
static struct dasics_compartment fault_recovery_compartment;
static u64 fault_recovery_value = 0x12345678;
static u64 fault_recovery_after;

static bool fault_recovery_hw_states_equal(const struct dasics_hw_state *left,
			       const struct dasics_hw_state *right)
{
	return left->libcfg == right->libcfg &&
	       left->jumpcfg == right->jumpcfg &&
	       left->dmaincall == right->dmaincall &&
	       left->dretpc == right->dretpc &&
	       left->dretpcactz == right->dretpcactz &&
	       !memcmp(left->lib_lo, right->lib_lo, sizeof(left->lib_lo)) &&
	       !memcmp(left->lib_hi, right->lib_hi, sizeof(left->lib_hi)) &&
	       !memcmp(left->jump_lo, right->jump_lo, sizeof(left->jump_lo)) &&
	       !memcmp(left->jump_hi, right->jump_hi, sizeof(left->jump_hi));
}

static int fault_recovery_expect_parent(const struct dasics_hw_state *parent)
{
	struct dasics_hw_state actual;
	int ret;

	ret = dasics_hw_save(&actual);
	if (ret)
		return ret;
	return fault_recovery_hw_states_equal(parent, &actual) ? 0 : -EIO;
}

static int fault_recovery_run_fault(void *target, unsigned long a0,
			unsigned long a1, unsigned long reason,
			long expected_error,
			const struct dasics_hw_state *parent)
{
	struct dasics_call_regs regs = { .a0 = a0, .a1 = a1 };
	struct dasics_region after_region = {
		.base = (unsigned long)&fault_recovery_after,
		.size = sizeof(fault_recovery_after),
		.perms = DASICS_REGION_WRITE,
	};
	struct dasics_call_policy policy = {
		.callee = &fault_recovery_compartment,
		.target = target,
		.regions = &after_region,
		.nr_regions = 1,
		.stack_size = 512,
	};
	long ret;

	fault_recovery_after = 0;
	memset(&fault_recovery_frame, 0, sizeof(fault_recovery_frame));
	ret = dasics_call(&fault_recovery_frame, &policy, &regs);
	if (ret != expected_error || fault_recovery_after || !fault_recovery_frame.fault.valid ||
	    fault_recovery_frame.fault.reason != reason ||
	    fault_recovery_frame.state != DASICS_CALL_FRAME_CLEANED ||
	    fault_recovery_frame.parent_saved || fault_recovery_frame.preempt_held)
		goto fail;
	return fault_recovery_expect_parent(parent);

fail:
	pr_err("[DASICS-FAULT-RECOVERY]: unexpected result ret=%ld after=%llu valid=%u reason=%lu state=%u saved=%u preempt=%u\n",
	       ret, fault_recovery_after, fault_recovery_frame.fault.valid, fault_recovery_frame.fault.reason,
	       fault_recovery_frame.state, fault_recovery_frame.parent_saved, fault_recovery_frame.preempt_held);
	return -EINVAL;
}

static void noinline fault_recovery_forbidden_target(void)
{
	fault_recovery_value = 0xbad;
}

static int __init fault_recovery_tmod_init(void)
{
	struct dasics_hw_state parent;
	u64 original = fault_recovery_value;
	int ret;

	ret = dasics_compartment_init_module(&fault_recovery_compartment, fault_recovery_fault_load);
	if (ret)
		goto out;
	ret = dasics_hw_save(&parent);
	if (ret)
		goto out;

	ret = fault_recovery_run_fault(fault_recovery_fault_load, (unsigned long)&fault_recovery_value,
			   (unsigned long)&fault_recovery_after, DASICS_FAULT_LOAD,
			   -EFAULT, &parent);
	if (ret)
		goto out;
	pr_info("[DASICS-FAULT-RECOVERY]: PASS out-of-bounds read unwound\n");

	ret = fault_recovery_run_fault(fault_recovery_fault_store, (unsigned long)&fault_recovery_value,
			   (unsigned long)&fault_recovery_after, DASICS_FAULT_STORE,
			   -EFAULT, &parent);
	if (ret || fault_recovery_value != original)
		goto out;
	pr_info("[DASICS-FAULT-RECOVERY]: PASS out-of-bounds write unwound\n");

	ret = fault_recovery_run_fault(fault_recovery_fault_jump,
			   (unsigned long)fault_recovery_forbidden_target,
			   (unsigned long)&fault_recovery_after, DASICS_FAULT_JUMP,
			   -EPERM, &parent);
	if (ret || fault_recovery_value != original)
		goto out;
	pr_info("[DASICS-FAULT-RECOVERY]: PASS illegal jump unwound\n");

	ret = fault_recovery_run_fault(fault_recovery_fault_stack_low,
			   (unsigned long)&fault_recovery_after, 0,
			   DASICS_FAULT_STORE, -EFAULT, &parent);
	if (ret)
		goto out;
	pr_info("[DASICS-FAULT-RECOVERY]: PASS stack lower bound rejected underflow\n");

	ret = fault_recovery_run_fault(fault_recovery_fault_stack_high,
			   (unsigned long)&fault_recovery_after, 0,
			   DASICS_FAULT_STORE, -EFAULT, &parent);
	if (ret)
		goto out;
	pr_info("[DASICS-FAULT-RECOVERY]: PASS stack upper bound protected trusted caller frame\n");

out:
	dasics_call_finish(&fault_recovery_frame);
	dasics_compartment_destroy(&fault_recovery_compartment);
	if (ret) {
		pr_err("[DASICS-FAULT-RECOVERY]: FAIL fault recovery tests: %d\n", ret);
		return ret;
	}
	pr_info("[DASICS-FAULT-RECOVERY]: PASS all fault recovery tests\n");
	return 0;
}

static void __exit fault_recovery_tmod_exit(void)
{
	dasics_compartment_destroy(&fault_recovery_compartment);
}

module_init(fault_recovery_tmod_init);
module_exit(fault_recovery_tmod_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DASICS fault recovery fault recovery tests");
