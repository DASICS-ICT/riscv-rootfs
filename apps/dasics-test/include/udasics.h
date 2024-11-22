#ifndef _UDASICS_H_
#define _UDASICS_H_

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include "ucsr.h"
#include "uattr.h"
#include "usyscall.h"

/* Add dasics exceptions */
#define EXC_DASICS_ECALL_FAULT     1
#define EXC_DASICS_LOAD_FAULT      2
#define EXC_DASICS_STORE_FAULT     3
#define EXC_DASICS_JUMP_FAULT      4
#define EXC_MPK_LOAD_FAULT         5
#define EXC_MPK_STORE_FAULT        6

/* DASICS csrs */
#define CSR_DUMBOUND        0x9e1
#define CSR_DSMBOUND        0xbc1
/* DASICS Lib csrs */
#define CSR_DMBOUND0      0x890
#define CSR_DMBOUND1      0x891
#define CSR_DMBOUND2      0x892
#define CSR_DMBOUND3      0x893
#define CSR_DMBOUND4      0x894
#define CSR_DMBOUND5      0x895
#define CSR_DMBOUND6      0x896
#define CSR_DMBOUND7      0x897
#define CSR_DMBOUND8      0x898
#define CSR_DMBOUND9      0x899
#define CSR_DMBOUND10     0x89a
#define CSR_DMBOUND11     0x89b
#define CSR_DMBOUND12     0x89c
#define CSR_DMBOUND13     0x89d
#define CSR_DMBOUND14     0x89e
#define CSR_DMBOUND15     0x89f
#define CSR_DMBOUND16     0x8a0
#define CSR_DMBOUND17     0x8a1
#define CSR_DMBOUND18     0x8a2
#define CSR_DMBOUND19     0x8a3
#define CSR_DMBOUND20     0x8a4
#define CSR_DMBOUND21     0x8a5
#define CSR_DMBOUND22     0x8a6
#define CSR_DMBOUND23     0x8a7
#define CSR_DMBOUND24     0x8a8
#define CSR_DMBOUND25     0x8a9
#define CSR_DMBOUND26     0x8aa
#define CSR_DMBOUND27     0x8ab
#define CSR_DMBOUND28     0x8ac
#define CSR_DMBOUND29     0x8ad
#define CSR_DMBOUND30     0x8ae
#define CSR_DMBOUND31     0x8af

#define CSR_DMAINCALL       0x8b0
#define CSR_DRETURNPC       0x8b1
#define CSR_DFZRETURN       0x8b2
#define CSR_DFREASON        0x8b3

#define CSR_DJBOUND0      0x8c0
#define CSR_DJBOUND1      0x8c1
#define CSR_DJBOUND2      0x8c2
#define CSR_DJBOUND3      0x8c3
#define CSR_DJBOUND4      0x8c4
#define CSR_DJBOUND5      0x8c5
#define CSR_DJBOUND6      0x8c6
#define CSR_DJBOUND7      0x8c7

/* DASICS Main cfg */
#define DASICS_MAINCFG_MASK    0xfUL
#define DASICS_MAINCFG_ECALLF  0x8UL
#define DASICS_MAINCFG_JUMPF   0x4UL
#define DASICS_MAINCFG_LOADF   0x2UL
#define DASICS_MAINCFG_STOREF  0x1UL

/* DASICS Mem cfg */
#define DASICS_MEMCFG_WIDTH 32
#define DASICS_MEMCFG_MASK  0xfUL
#define DASICS_MEMCFG_V     0x8UL
#define DASICS_MEMCFG_U     0x4UL
#define DASICS_MEMCFG_R     0x2UL
#define DASICS_MEMCFG_W     0x1UL

/* DASICS Jmp cfg */
#define DASICS_JMPCFG_WIDTH 8
#define DASICS_JMPCFG_V    	0x8UL

#define CONCAT(TYPE,OP) TYPE##_BOUND_REG_##OP

#define MEM_BOUND_REG_READ(bound,idx)   \
        case idx:  \
            bound = csr_read(0x890 + idx);  \
            break;

#define MEM_BOUND_REG_WRITE(bound,idx)   \
        case idx:  \
            csr_write(0x890 + idx, bound);  \
            break;

#define MEM_BOUND_LOOKUP(BOUND,IDX,OP) \
        switch (IDX) \
        {               \
            CONCAT(MEM,OP)(BOUND,0);  \
            CONCAT(MEM,OP)(BOUND,1);  \
            CONCAT(MEM,OP)(BOUND,2);  \
            CONCAT(MEM,OP)(BOUND,3);  \
            CONCAT(MEM,OP)(BOUND,4);  \
            CONCAT(MEM,OP)(BOUND,5);  \
            CONCAT(MEM,OP)(BOUND,6);  \
            CONCAT(MEM,OP)(BOUND,7);  \
            CONCAT(MEM,OP)(BOUND,8);  \
            CONCAT(MEM,OP)(BOUND,9);  \
            CONCAT(MEM,OP)(BOUND,10); \
            CONCAT(MEM,OP)(BOUND,11); \
            CONCAT(MEM,OP)(BOUND,12); \
            CONCAT(MEM,OP)(BOUND,13); \
            CONCAT(MEM,OP)(BOUND,14); \
            CONCAT(MEM,OP)(BOUND,15); \
            CONCAT(MEM,OP)(BOUND,16); \
            CONCAT(MEM,OP)(BOUND,17); \
            CONCAT(MEM,OP)(BOUND,18); \
            CONCAT(MEM,OP)(BOUND,19); \
            CONCAT(MEM,OP)(BOUND,20); \
            CONCAT(MEM,OP)(BOUND,21); \
            CONCAT(MEM,OP)(BOUND,22); \
            CONCAT(MEM,OP)(BOUND,23); \
            CONCAT(MEM,OP)(BOUND,24); \
            CONCAT(MEM,OP)(BOUND,25); \
            CONCAT(MEM,OP)(BOUND,26); \
            CONCAT(MEM,OP)(BOUND,27); \
            CONCAT(MEM,OP)(BOUND,28); \
            CONCAT(MEM,OP)(BOUND,29); \
            CONCAT(MEM,OP)(BOUND,30); \
            CONCAT(MEM,OP)(BOUND,31); \
            default: \
                printf("\x1b[31m%s\x1b[0m","[DASICS]Error: out of membound register range\n"); \
        }

#define JMP_BOUND_REG_READ(bound,idx)   \
        case idx:  \
            bound = csr_read(0x8c0 + idx);  \
            break;

#define JMP_BOUND_REG_WRITE(bound,idx)   \
        case idx:  \
            csr_write(0x8c0 + idx, bound);  \
            break;

#define JMP_BOUND_LOOKUP(BOUND,IDX,OP) \
        switch (IDX) \
        {               \
            CONCAT(JMP,OP)(BOUND,0);  \
            CONCAT(JMP,OP)(BOUND,1);  \
            CONCAT(JMP,OP)(BOUND,2);  \
            CONCAT(JMP,OP)(BOUND,3);  \
            CONCAT(JMP,OP)(BOUND,4);  \
            CONCAT(JMP,OP)(BOUND,5);  \
            CONCAT(JMP,OP)(BOUND,6);  \
            CONCAT(JMP,OP)(BOUND,7);  \
            default: \
                printf("\x1b[31m%s\x1b[0m","[DASICS]Error: out of jmpbound register range\n"); \
        }


#define cal_dasics_bound_val(lo, hi, cfg)           \
     (((uint64_t)    lo  & ((1UL<<39)-1)) |         \
     (((uint64_t)(hi-lo) & ((1UL<<21)-1)) <<  39) | \
     (((uint64_t)   cfg  & ((1UL<< 4)-1)) <<  60))

#define get_dasics_bound_lo(bound)           \
     ((uint64_t)bound & ((1UL<<39)-1))

#define get_dasics_bound_hi(bound)           \
     (get_dasics_bound_lo(bound) + (((uint64_t)bound >> 39) & ((1UL<<21)-1)))

#define get_dasics_bound_cfg(bound)          \
     ((uint64_t)bound >> 60)


// TODO: Add UmaincallTypes
typedef enum {
    Umaincall_PRINT,
    Umaincall_SETAZONERTPC,
    Umaincall_UNKNOWN
} UmaincallTypes;

void register_udasics(uint64_t funcptr);
void unregister_udasics(void);
uint64_t dasics_umaincall_helper(UmaincallTypes type, ...);
void     dasics_ufault_handler(void);

int32_t  dasics_membound_alloc(uint64_t cfg, uint64_t lo, uint64_t hi);
uint64_t dasics_membound_get(int32_t idx);
int32_t  dasics_membound_set(int32_t idx, uint64_t val);

int32_t  dasics_jmpbound_alloc(uint64_t lo, uint64_t hi);
uint64_t dasics_jmpbound_get(int32_t idx);
int32_t  dasics_jmpbound_set(int32_t idx, uint64_t val);

#define dasics_membound_free(idx) dasics_membound_set(idx,0)
#define dasics_jmpbound_free(idx) dasics_jmpbound_set(idx,0)

// extern uint64_t umaincall_helper;
extern void dasics_ufault_entry(void);
extern uint64_t dasics_umaincall(UmaincallTypes type, ...);
extern void lib_call(void* func_name);
extern void azone_call(void* func_name);

#endif
