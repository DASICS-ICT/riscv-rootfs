#ifndef _MEMORY_INCLUDE_MEM_H
#define _MEMORY_INCLUDE_MEM_H

#include <list.h>
#include <my_stdio.h>
#define PAGE_SIZE 0x1000UL
#define MAX_NUM 256

#define MAX_MEM (PAGE_SIZE * MAX_NUM)

typedef struct page_node
{
    list_head list;
    uint64_t start;
}page_node_t;

#define GET_MEM_NODE(baseAddr, mm) (((baseAddr) - (mm->mem_begin)) / PAGE_SIZE)

// the mem_struct of all the lib
typedef struct mem_struct
{
    list_head mem_used;
    list_head mem_unused;

    // the mem_begin and mem_end
    uint64_t mem_begin, mem_end;
    uint64_t high;

    // a array
    page_node_t * page_manager;

    uint64_t (*malloc)(struct mem_struct *, uint64_t);
    uint64_t (*free)(struct mem_struct *, uint64_t);
    uint64_t (*realloc)(struct mem_struct *, uint64_t, uint64_t);
} mem_struct_t;

uint64_t dasics_malloc(mem_struct_t * mm, uint64_t size);
uint64_t dasics_free(mem_struct_t * mm, uint64_t addr);
uint64_t dasics_realloc(mem_struct_t * mm, uint64_t addr, uint64_t size);

void init_mm_struct();

#ifdef DASICS_DEBUG
static inline void debug_print_mm(mem_struct_t * mm)
{
    my_printf("++++++++++++++++   BEGIN DEBUG mem_struct_t  ++++++++++++++++\n");

    my_printf("mem_begin: 0x%lx\n", mm->mem_begin);
    my_printf("mem_end: 0x%lx\n", mm->mem_end);

    my_printf("++++++++++++++++   END DEBUG mem_struct_t  ++++++++++++++++\n");
}
#endif

#endif