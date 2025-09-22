#ifndef _UINTR_H_
#define _UINTR_H_

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include "ucsr.h"

void prepare_u_intr(void);
void clear_u_intr(void);
extern void u_intr_entry(void);
extern void u_intr_handler(void);

#define CAUSE_IRQ_U_EXT ((uint64_t)((1ULL<<63) | 8))
#define CAUSE_IRQ_U_TIMER ((uint64_t)((1ULL<<63) | 4))

#endif