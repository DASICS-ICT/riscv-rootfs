#ifndef __INCLUDE_ASM_CSR_H
#define __INCLUDE_ASM_CSR_H

/* U state csrs */
#define CSR_USTATUS         0x000
#define CSR_UIE             0x004
#define CSR_UTVEC           0x005
#define CSR_USCRATCH        0x040
#define CSR_UEPC            0x041
#define CSR_UCAUSE          0x042
#define CSR_UTVAL           0x043
#define CSR_UIP             0x044

/* DASICS Main csrs */
#define CSR_DUMCFG          0x5c0
#define CSR_DUMBOUNDHI      0x5c1
#define CSR_DUMBOUNDLO      0x5c2

/* DASICS Main cfg */
#define DASICS_MAINCFG_MASK 0xfUL
#define DASICS_UCFG_CLS     0x8UL
#define DASICS_SCFG_CLS     0x4UL
#define DASICS_UCFG_ENA     0x2UL
#define DASICS_SCFG_ENA     0x1UL

/* DASICS Lib csrs */
#define CSR_DLCFG0          0x881
#define CSR_DLCFG1          0x882

#define CSR_DLBOUND0        0x883
#define CSR_DLBOUND1        0x884
#define CSR_DLBOUND2        0x885
#define CSR_DLBOUND3        0x886
#define CSR_DLBOUND4        0x887
#define CSR_DLBOUND5        0x888
#define CSR_DLBOUND6        0x889
#define CSR_DLBOUND7        0x88a
#define CSR_DLBOUND8        0x88b
#define CSR_DLBOUND9        0x88c
#define CSR_DLBOUND10       0x88d
#define CSR_DLBOUND11       0x88e
#define CSR_DLBOUND12       0x88f
#define CSR_DLBOUND13       0x890
#define CSR_DLBOUND14       0x891
#define CSR_DLBOUND15       0x892
#define CSR_DLBOUND16       0x893
#define CSR_DLBOUND17       0x894
#define CSR_DLBOUND18       0x895
#define CSR_DLBOUND19       0x896
#define CSR_DLBOUND20       0x897
#define CSR_DLBOUND21       0x898
#define CSR_DLBOUND22       0x899
#define CSR_DLBOUND23       0x89a
#define CSR_DLBOUND24       0x89b
#define CSR_DLBOUND25       0x89c
#define CSR_DLBOUND26       0x89d
#define CSR_DLBOUND27       0x89e
#define CSR_DLBOUND28       0x89f
#define CSR_DLBOUND29       0x8a0
#define CSR_DLBOUND30       0x8a1
#define CSR_DLBOUND31       0x8a2

#define CSR_DMAINCALL       0x8a3
#define CSR_DRETURNPC       0x8a4
#define CSR_DFZRETURN       0x8a5



#define csr_read(reg) ({ unsigned long __tmp; \
  asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
  __tmp; })

#define csr_write(reg, val) ({ \
  asm volatile ("csrw " #reg ", %0" :: "rK"(val)); })

#endif