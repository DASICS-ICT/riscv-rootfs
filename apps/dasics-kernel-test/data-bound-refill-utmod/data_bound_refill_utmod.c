// SPDX-License-Identifier: GPL-2.0-only

#include <linux/module.h>

#include "data_bound_refill_utmod.h"

u64 data_bound_refill_touch_cells(struct data_bound_refill_cell *cells, u64 count)
{
	u64 sum = 0;
	u64 i;

	for (i = 0; i < count; i++)
		cells[i].value = i + 1;
	for (i = count; i > 0; i--)
		sum += cells[i - 1].value;
	return sum;
}
EXPORT_SYMBOL(data_bound_refill_touch_cells);

int data_bound_refill_write_one(u64 *value)
{
	*value = 0xd3;
	return 0;
}
EXPORT_SYMBOL(data_bound_refill_write_one);

static int __init data_bound_refill_utmod_init(void)
{
	return 0;
}

static void __exit data_bound_refill_utmod_exit(void)
{
}

module_init(data_bound_refill_utmod_init);
module_exit(data_bound_refill_utmod_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DASICS data-bound refill untrusted refill targets");
