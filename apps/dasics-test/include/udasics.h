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
#define EXC_DASICS_JUMP_FAULT     4
#define EXC_MPK_LOAD_FAULT          5
#define EXC_MPK_STORE_FAULT         6
/* DASICS csrs */
#define CSR_SFETCHCTL       0x9e0
#define CSR_DUMCFG          0x9e1
#define CSR_DUMBOUNDLO      0x9e2
#define CSR_DUMBOUNDHI      0x9e3

/* DASICS Main cfg */
#define DASICS_SMAINCFG_MASK 0x3ffUL
#define DASICS_UMAINCFG_MASK 0x3eUL
#define DASICS_MAINCFG_MASK DASICS_SMAINCFG_MASK
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
#define CSR_DLBOUND4LO      0x898
#define CSR_DLBOUND4HI      0x899
#define CSR_DLBOUND5LO      0x89a
#define CSR_DLBOUND5HI      0x89b
#define CSR_DLBOUND6LO      0x89c
#define CSR_DLBOUND6HI      0x89d
#define CSR_DLBOUND7LO      0x89e
#define CSR_DLBOUND7HI      0x89f
#define CSR_DLBOUND8LO      0x8a0
#define CSR_DLBOUND8HI      0x8a1
#define CSR_DLBOUND9LO      0x8a2
#define CSR_DLBOUND9HI      0x8a3
#define CSR_DLBOUND10LO     0x8a4
#define CSR_DLBOUND10HI     0x8a5
#define CSR_DLBOUND11LO     0x8a6
#define CSR_DLBOUND11HI     0x8a7
#define CSR_DLBOUND12LO     0x8a8
#define CSR_DLBOUND12HI     0x8a9
#define CSR_DLBOUND13LO     0x8aa
#define CSR_DLBOUND13HI     0x8ab
#define CSR_DLBOUND14LO     0x8ac
#define CSR_DLBOUND14HI     0x8ad
#define CSR_DLBOUND15LO     0x8ae
#define CSR_DLBOUND15HI     0x8af

#define CSR_DMAINCALL       0x8b0
#define CSR_DRETURNPC       0x8b1
#define CSR_DFZRETURN       0x8b2
#define CSR_DFREASON        0x8b3

#define CSR_DJBOUND0LO      0x8c0
#define CSR_DJBOUND0HI      0x8c1
#define CSR_DJBOUND1LO      0x8c2
#define CSR_DJBOUND1HI      0x8c3
#define CSR_DJBOUND2LO      0x8c4
#define CSR_DJBOUND2HI      0x8c5
#define CSR_DJBOUND3LO      0x8c6
#define CSR_DJBOUND3HI      0x8c7
#define CSR_DJCFG           0x8c8

/* DASICS Lib cfg */
#define DASICS_LIBCFG_WIDTH 16
#define DASICS_LIBCFG_MASK  0xfUL
#define DASICS_LIBCFG_V     0x8UL
#define DASICS_LIBCFG_R     0x2UL
#define DASICS_LIBCFG_W     0x1UL

#define DASICS_BOUND_GRANULE 8UL
#define DASICS_BOUND_ALIGN_DOWN(addr) \
    ((uint64_t)(addr) & ~(DASICS_BOUND_GRANULE - 1UL))
#define DASICS_BOUND_ALIGN_UP(addr) \
    (((uint64_t)(addr) + DASICS_BOUND_GRANULE - 1UL) & \
     ~(DASICS_BOUND_GRANULE - 1UL))

#define DASICS_JUMPCFG_WIDTH 	4
#define DASICS_JUMPCFG_MASK 	0xffffUL
#define DASICS_JUMPCFG_V    	0x1UL

// TODO: Add UmaincallTypes
typedef enum {
    Umaincall_PRINT,
    Umaincall_SETAZONERTPC,
    Umaincall_UNKNOWN
} UmaincallTypes;

#define DASICS_COMPLETE_APP_CONTROL_MAGIC 0x4644494150504354UL
#define DASICS_COMPLETE_APP_CONTROL_SET 0x534554UL
#define DASICS_COMPLETE_APP_CONTROL_RESTORE 0x525354UL
#define DASICS_COMPLETE_APP_CONTROL_QUERY 0x515259UL
#define DASICS_COMPLETE_APP_STAGE_A 0UL
#define DASICS_COMPLETE_APP_STAGE_B 1UL
#define DASICS_COMPLETE_APP_STAGE_RESTORE 2UL
#define DASICS_COMPLETE_APP_CFG_OFF 0UL
#define DASICS_COMPLETE_APP_CFG_UENA DASICS_UCFG_ENA
#define DASICS_COMPLETE_APP_GETPID 306UL

#ifdef DASICS_N_EXTENSION_PROFILE
#define DASICS_N_EXTENSION_SYSCALL_RECORDS 4UL
#define DASICS_N_EXTENSION_TRAP_RECORDS 32UL
#define DASICS_N_EXTENSION_UCHECK_CAUSE 24UL
#define DASICS_N_EXTENSION_USTATUS_UIE 0x1UL
#define DASICS_N_EXTENSION_USTATUS_UPIE 0x10UL

typedef struct {
    uint64_t ustatus;
    uint64_t uie;
    uint64_t utvec;
    uint64_t uscratch;
    uint64_t uepc;
    uint64_t ucause;
    uint64_t utval;
    uint64_t uip;
    uint64_t dfreason;
    uint64_t recovery;
    uint64_t gpr[32];
} dasics_n_extension_trap_record_t;

typedef struct {
    uint64_t ustatus;
    uint64_t uepc;
    uint64_t ucause;
    uint64_t utval;
    uint64_t dfreason;
    uint64_t permitted;
    int64_t result;
} dasics_n_extension_syscall_record_t;

extern volatile uint64_t dasics_n_extension_syscall_trap_count;
extern volatile uint64_t dasics_n_extension_syscall_permitted_count;
extern volatile uint64_t dasics_n_extension_syscall_denied_count;
extern volatile dasics_n_extension_syscall_record_t
    dasics_n_extension_syscall_records[DASICS_N_EXTENSION_SYSCALL_RECORDS];
extern volatile uint64_t dasics_n_extension_trap_count;
extern volatile dasics_n_extension_trap_record_t
    dasics_n_extension_trap_records[DASICS_N_EXTENSION_TRAP_RECORDS];
long dasics_n_extension_syscall_handler(SYSCALL_ARGS);
void dasics_n_extension_runtime_init(void);
void dasics_n_extension_runtime_fini(void);
void dasics_n_extension_dump_context(void);
#endif

void register_udasics(uint64_t funcptr);
void unregister_udasics(void);
void set_ufault_print_info(uint64_t status);
uint64_t dasics_umaincall_helper(UmaincallTypes type, ...);
void     dasics_ufault_handler(void);
int32_t  dasics_libcfg_alloc(uint64_t cfg, uint64_t lo, uint64_t hi);
int32_t  dasics_libcfg_free(int32_t idx);
uint32_t dasics_libcfg_get(int32_t idx);
void dasics_print_cfg_register(int32_t idx);
int32_t dasics_jumpcfg_alloc(uint64_t lo, uint64_t hi); 
int32_t dasics_jumpcfg_free(int32_t idx);

// extern uint64_t umaincall_helper;
extern void dasics_ufault_entry(void);
extern uint64_t dasics_umaincall(UmaincallTypes type, ...);
extern int lib_call(int (*func)(void));
extern void lib_call_1(uint64_t arg, void* func_name);
extern void azone_call(void* func_name);
long dasics_complete_app_getpid(void);
long dasics_complete_app_control(uint64_t command, uint64_t pid,
                                 uint64_t stage, uint64_t value);

#endif
