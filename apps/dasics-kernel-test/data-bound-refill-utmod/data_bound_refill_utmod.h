/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _DATA_BOUND_REFILL_UTMOD_H
#define _DATA_BOUND_REFILL_UTMOD_H

#include <linux/types.h>

#define DATA_BOUND_REFILL_NR_CELLS 20

struct data_bound_refill_cell {
	u64 value;
	u64 gap;
};

u64 data_bound_refill_touch_cells(struct data_bound_refill_cell *cells, u64 count);
int data_bound_refill_write_one(u64 *value);

#endif
