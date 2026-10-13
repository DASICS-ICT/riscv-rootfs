// SPDX-License-Identifier: GPL-2.0-only

#include <linux/dasics.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>

#include "../data-bound-refill-utmod/data_bound_refill_utmod.h"

static struct dasics_call_frame data_bound_refill_frame;
static struct dasics_compartment data_bound_refill_compartment;
static struct data_bound_refill_cell data_bound_refill_cells[DATA_BOUND_REFILL_NR_CELLS];
static struct dasics_region data_bound_refill_regions[DATA_BOUND_REFILL_NR_CELLS];

static bool data_bound_refill_hw_states_equal(const struct dasics_hw_state *left,
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

static int data_bound_refill_expect_parent(const struct dasics_hw_state *parent)
{
	struct dasics_hw_state actual;
	int ret;

	ret = dasics_hw_save(&actual);
	if (ret)
		return ret;
	return data_bound_refill_hw_states_equal(parent, &actual) ? 0 : -EIO;
}

static void data_bound_refill_init_regions(unsigned int last_perms)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(data_bound_refill_regions); i++) {
		data_bound_refill_regions[i].base = (unsigned long)&data_bound_refill_cells[i].value;
		data_bound_refill_regions[i].size = sizeof(data_bound_refill_cells[i].value);
		data_bound_refill_regions[i].perms = i == ARRAY_SIZE(data_bound_refill_regions) - 1 ?
			last_perms : DASICS_REGION_READ | DASICS_REGION_WRITE;
	}
}

static int data_bound_refill_test_refill(const struct dasics_hw_state *parent)
{
	struct dasics_call_regs regs = { };
	struct dasics_call_policy policy = {
		.callee = &data_bound_refill_compartment,
		.target = data_bound_refill_touch_cells,
		.regions = data_bound_refill_regions,
		.nr_regions = ARRAY_SIZE(data_bound_refill_regions),
		.stack_size = 512,
	};
	unsigned int i;
	long ret;

	memset(data_bound_refill_cells, 0, sizeof(data_bound_refill_cells));
	data_bound_refill_init_regions(DASICS_REGION_READ | DASICS_REGION_WRITE);
	memset(&data_bound_refill_frame, 0, sizeof(data_bound_refill_frame));
	regs.a0 = (unsigned long)data_bound_refill_cells;
	regs.a1 = ARRAY_SIZE(data_bound_refill_cells);
	ret = dasics_call(&data_bound_refill_frame, &policy, &regs);
	if (ret || regs.ret_a0 != 210 || data_bound_refill_frame.fault.valid ||
	    data_bound_refill_frame.data_refills < 6 ||
	    data_bound_refill_frame.data_misses != data_bound_refill_frame.data_refills)
		return -EINVAL;
	for (i = 0; i < ARRAY_SIZE(data_bound_refill_cells); i++) {
		if (data_bound_refill_cells[i].value != i + 1)
			return -EIO;
	}
	ret = data_bound_refill_expect_parent(parent);
	if (!ret)
		pr_info("[DASICS-DATA-BOUND-REFILL]: PASS %zu regions refilled %u misses and retried\n",
			ARRAY_SIZE(data_bound_refill_regions), data_bound_refill_frame.data_refills);
	return ret;
}

static int data_bound_refill_test_permission_mismatch(const struct dasics_hw_state *parent)
{
	struct dasics_call_regs regs = { };
	struct dasics_call_policy policy = {
		.callee = &data_bound_refill_compartment,
		.target = data_bound_refill_write_one,
		.regions = &data_bound_refill_regions[DATA_BOUND_REFILL_NR_CELLS - 1],
		.nr_regions = 1,
		.stack_size = 512,
	};
	u64 original = data_bound_refill_cells[DATA_BOUND_REFILL_NR_CELLS - 1].value;
	long ret;

	data_bound_refill_init_regions(DASICS_REGION_READ);
	memset(&data_bound_refill_frame, 0, sizeof(data_bound_refill_frame));
	regs.a0 = (unsigned long)&data_bound_refill_cells[DATA_BOUND_REFILL_NR_CELLS - 1].value;
	ret = dasics_call(&data_bound_refill_frame, &policy, &regs);
	if (ret != -EFAULT || !data_bound_refill_frame.fault.valid ||
	    data_bound_refill_frame.fault.reason != DASICS_FAULT_STORE ||
	    data_bound_refill_cells[DATA_BOUND_REFILL_NR_CELLS - 1].value != original)
		return -EINVAL;
	ret = data_bound_refill_expect_parent(parent);
	if (!ret)
		pr_info("[DASICS-DATA-BOUND-REFILL]: PASS write permission mismatch rejected\n");
	return ret;
}

static int data_bound_refill_test_pinned_exhaustion(const struct dasics_hw_state *parent)
{
	struct dasics_call_policy policy = {
		.callee = &data_bound_refill_compartment,
		.target = data_bound_refill_touch_cells,
		.regions = data_bound_refill_regions,
		.nr_regions = ARRAY_SIZE(data_bound_refill_regions),
		.stack_size = 512,
	};
	unsigned long address = data_bound_refill_regions[DATA_BOUND_REFILL_NR_CELLS - 1].base;
	unsigned int i;
	int ret;

	data_bound_refill_init_regions(DASICS_REGION_READ | DASICS_REGION_WRITE);
	memset(&data_bound_refill_frame, 0, sizeof(data_bound_refill_frame));
	ret = dasics_call_prepare(&data_bound_refill_frame, &policy);
	if (ret)
		return ret;
	for (i = 0; i < ARRAY_SIZE(data_bound_refill_frame.data_bounds.entries); i++) {
		struct dasics_bound_entry *entry = &data_bound_refill_frame.data_bounds.entries[i];

		if (entry->active && entry->resident_slot >= 0)
			entry->flags |= DASICS_BOUND_PINNED;
	}
	ret = dasics_call_test_transition(&data_bound_refill_frame,
					  DASICS_CALL_FRAME_ENTERED);
	if (ret)
		goto finish;
	ret = dasics_call_test_handle_trap((unsigned long)data_bound_refill_touch_cells,
					   address, DASICS_FAULT_STORE,
					   EXC_DASICS_SCHECK_FAULT);
	if (ret != DASICS_TRAP_TERMINAL || !data_bound_refill_frame.fault.valid ||
	    data_bound_refill_frame.data_refills)
		ret = -EINVAL;
	else
		ret = 0;

finish:
	dasics_call_finish(&data_bound_refill_frame);
	if (!ret)
		ret = data_bound_refill_expect_parent(parent);
	if (!ret)
		pr_info("[DASICS-DATA-BOUND-REFILL]: PASS pinned slot exhaustion failed closed\n");
	return ret;
}

static int data_bound_refill_test_unknown_address(const struct dasics_hw_state *parent)
{
	struct dasics_call_policy policy = {
		.callee = &data_bound_refill_compartment,
		.target = data_bound_refill_touch_cells,
		.regions = data_bound_refill_regions,
		.nr_regions = ARRAY_SIZE(data_bound_refill_regions),
		.stack_size = 512,
	};
	unsigned long address;
	int ret;

	data_bound_refill_init_regions(DASICS_REGION_READ | DASICS_REGION_WRITE);
	address = data_bound_refill_regions[DATA_BOUND_REFILL_NR_CELLS - 1].base +
		 2 * sizeof(struct data_bound_refill_cell);
	memset(&data_bound_refill_frame, 0, sizeof(data_bound_refill_frame));
	ret = dasics_call_prepare(&data_bound_refill_frame, &policy);
	if (ret)
		return ret;
	ret = dasics_call_test_transition(&data_bound_refill_frame,
					  DASICS_CALL_FRAME_ENTERED);
	if (ret)
		goto finish;
	ret = dasics_call_test_handle_trap((unsigned long)data_bound_refill_touch_cells,
					   address, DASICS_FAULT_LOAD,
					   EXC_DASICS_SCHECK_FAULT);
	if (ret != DASICS_TRAP_TERMINAL || !data_bound_refill_frame.fault.valid ||
	    data_bound_refill_frame.fault.address != address || data_bound_refill_frame.data_refills)
		ret = -EINVAL;
	else
		ret = 0;

finish:
	dasics_call_finish(&data_bound_refill_frame);
	if (!ret)
		ret = data_bound_refill_expect_parent(parent);
	if (!ret)
		pr_info("[DASICS-DATA-BOUND-REFILL]: PASS unknown address failed closed\n");
	return ret;
}

static int __init data_bound_refill_tmod_init(void)
{
	struct dasics_hw_state original;
	struct dasics_hw_state parent;
	bool original_saved = false;
	int ret;

	ret = dasics_hw_save(&original);
	if (ret)
		goto out;
	original_saved = true;
	ret = dasics_compartment_init_module(&data_bound_refill_compartment, data_bound_refill_touch_cells);
	if (ret)
		goto out;
	ret = dasics_hw_save(&parent);
	if (ret)
		goto out;
	ret = data_bound_refill_test_refill(&parent);
	if (ret)
		goto out;
	ret = data_bound_refill_test_permission_mismatch(&parent);
	if (ret)
		goto out;
	ret = data_bound_refill_test_pinned_exhaustion(&parent);
	if (ret)
		goto out;
	ret = data_bound_refill_test_unknown_address(&parent);

out:
	dasics_call_finish(&data_bound_refill_frame);
	dasics_compartment_destroy(&data_bound_refill_compartment);
	if (original_saved) {
		int restore_ret = dasics_hw_restore(&original);

		if (!ret)
			ret = restore_ret;
	}
	if (ret) {
		pr_err("[DASICS-DATA-BOUND-REFILL]: FAIL data-bound refill tests: %d\n", ret);
		return ret;
	}
	pr_info("[DASICS-DATA-BOUND-REFILL]: PASS all data-bound refill tests\n");
	return 0;
}

static void __exit data_bound_refill_tmod_exit(void)
{
	dasics_compartment_destroy(&data_bound_refill_compartment);
}

module_init(data_bound_refill_tmod_init);
module_exit(data_bound_refill_tmod_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DASICS data-bound refill data-bound refill tests");
