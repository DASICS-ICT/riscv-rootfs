#ifndef _UDASICS_H_
#define _UDASICS_H_

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include "ucsr.h"
#include "uattr.h"
#include "usyscall.h"

/* Add dasics exceptions */
#define EXC_DASICS_UFETCH_FAULT     24
#define EXC_DASICS_SFETCH_FAULT     25
#define EXC_DASICS_ULOAD_FAULT      26
#define EXC_DASICS_SLOAD_FAULT      27
#define EXC_DASICS_USTORE_FAULT     28
#define EXC_DASICS_SSTORE_FAULT     29
#define EXC_DASICS_UECALL_FAULT     30
#define EXC_DASICS_SECALL_FAULT     31

/* DASICS csrs */
#define CSR_DUMCFG          0x9e0
#define CSR_DUMBOUNDLO      0x9e2
#define CSR_DUMBOUNDHI      0x9e3

/* DASICS Main cfg */
#define DASICS_MAINCFG_MASK 0xfUL
#define DASICS_UCFG_CLS     0x8UL
#define DASICS_SCFG_CLS     0x4UL
#define DASICS_UCFG_ENA     0x2UL
#define DASICS_SCFG_ENA     0x1UL

#define CSR_DLCFG0          0x880

#define CSR_DLBOUND0LO      0x890
#define CSR_DLBOUND0HI      0x891
#define CSR_DLBOUND1LO      0x892
#define CSR_DLBOUND1HI      0x893
#define CSR_DLBOUND2LO      0x894
#define CSR_DLBOUND2HI      0x895
#define CSR_DLBOUND3LO      0x896
#define CSR_DLBOUND3HI      0x897
// #define CSR_DLBOUND4LO      0x898
// #define CSR_DLBOUND4HI      0x899
// #define CSR_DLBOUND5LO      0x89a
// #define CSR_DLBOUND5HI      0x89b
// #define CSR_DLBOUND6LO      0x89c
// #define CSR_DLBOUND6HI      0x89d
// #define CSR_DLBOUND7LO      0x89e
// #define CSR_DLBOUND7HI      0x89f
// #define CSR_DLBOUND8LO      0x8a0
// #define CSR_DLBOUND8HI      0x8a1
// #define CSR_DLBOUND9LO      0x8a2
// #define CSR_DLBOUND9HI      0x8a3
// #define CSR_DLBOUND10LO     0x8a4
// #define CSR_DLBOUND10HI     0x8a5
// #define CSR_DLBOUND11LO     0x8a6
// #define CSR_DLBOUND11HI     0x8a7
// #define CSR_DLBOUND12LO     0x8a8
// #define CSR_DLBOUND12HI     0x8a9
// #define CSR_DLBOUND13LO     0x8aa
// #define CSR_DLBOUND13HI     0x8ab
// #define CSR_DLBOUND14LO     0x8ac
// #define CSR_DLBOUND14HI     0x8ad
// #define CSR_DLBOUND15LO     0x8ae
// #define CSR_DLBOUND15HI     0x8af

#define CSR_DMAINCALL       0x8b0
#define CSR_DRETURNPC       0x8b1
// #define CSR_DFZRETURN       0x8b2

#define CSR_DJBOUND0LO      0x8c0
#define CSR_DJBOUND0HI      0x8c1
// #define CSR_DJBOUND1LO      0x8c2
// #define CSR_DJBOUND1HI      0x8c3
// #define CSR_DJBOUND2LO      0x8c4
// #define CSR_DJBOUND2HI      0x8c5
// #define CSR_DJBOUND3LO      0x8c6
// #define CSR_DJBOUND3HI      0x8c7
#define CSR_DJCFG           0x8c8

/* DASICS Lib cfg */
#define DASICS_LIBCFG_WIDTH 4
#define DASICS_LIBCFG_MASK  0xfUL
#define DASICS_LIBCFG_V     0x8UL
#define DASICS_LIBCFG_R     0x2UL
#define DASICS_LIBCFG_W     0x1UL

#define DASICS_JUMPCFG_WIDTH 	1
#define DASICS_JUMPCFG_MASK 	0xffffUL
#define DASICS_JUMPCFG_V    	0x1UL

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
int32_t  dasics_libcfg_alloc(uint64_t cfg, uint64_t hi, uint64_t lo);
int32_t  dasics_libcfg_free(int32_t idx);
uint32_t dasics_libcfg_get(int32_t idx);
void dasics_print_cfg_register(int32_t idx);

// extern uint64_t umaincall_helper;
extern void dasics_ufault_entry(void);
extern uint64_t dasics_umaincall(UmaincallTypes type, ...);
extern void lib_call(void* func_name);
extern void azone_call(void* func_name);

#endif
