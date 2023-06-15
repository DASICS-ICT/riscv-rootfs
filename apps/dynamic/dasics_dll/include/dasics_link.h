#ifndef	_INCLUDE_DASICSLINK_H
#define	_INCLUDE_DASICSLINK_H
#include <elf.h>
#include <my_stdio.h>
#include <asm/dasics_ucontext.h>
#include <mem.h>

#ifdef RV32
    #define __ELF_NATIVE_CLASS 32
#else
    #define __ELF_NATIVE_CLASS 64
#endif

#define D_PTR(map,i) map->i->d_un.d_ptr

/* We use this macro to refer to ELF types independent of the native wordsize.
   `ElfW(TYPE)' is used in place of `Elf32_TYPE' or `Elf64_TYPE'.  */
#define ELFW(type)	_ElfW (ELF, __ELF_NATIVE_CLASS, type)

/* We use this macro to refer to ELF types independent of the native wordsize.
   `ElfW(TYPE)' is used in place of `Elf32_TYPE' or `Elf64_TYPE'.  */
#define ElfW(type)	_ElfW (Elf, __ELF_NATIVE_CLASS, type)
#define _ElfW(e,w,t)	_ElfW_1 (e, w, _##t)
#define _ElfW_1(e,w,t)	e##w##t
#define PLTREL  ElfW(Rela)


#define RANGE(X, Y, Z) (((X)>(Y)) && ((X)<(Z)))

/* Define the find result of the plt */
#define NOEXIST -1

/* macro used to call init */
typedef void (*init_t)(int, char **, char **);
#define DL_CALL_DT_INIT(start, argc, argv, env) \
 ((init_t) (start)) (argc, argv, env)


/* A data structure for a simple single linked list of strings.  */
struct libname_list
{
   const char *name;		/* Name requested (before search).  */
   struct libname_list *next;	/* Link to next name for this object.  */
   int dont_free;		/* Flag whether this element should be freed
            if the object is not entirely unloaded.  */
};


/*
 * The link map which is copyed from the riscv-gnu-toolchain/glibc/include/link.h
 * and we just need the valuable part.
 */
typedef struct dasics_link_map
{
   /* These first few members are part of the protocol with the debugger.
      This is the same format used in SVR4.  */

   ElfW(Addr) l_addr;		/* Difference between the address in the ELF
            file and the addresses in memory.  */
   char *l_name;		/* Absolute file name object was found in.  */
   ElfW(Dyn) *l_ld;		/* Dynamic section of the shared object.  */
   struct dasics_link_map *l_next, *l_prev; /* Chain of loaded objects.  */

   /* All following members are internal to the dynamic linker.
      They may change without notice.  */

   /* This is an element which is only ever different from a pointer to
      the very same copy of this type for ld.so when it is used in more
      than one namespace.  */
   struct dasics_link_map *l_real;

   /* Number of the namespace this link map belongs to.  */
   long l_ns;

   struct libname_list *l_libname;
   /* Indexed pointers to dynamic section.
      [0,DT_NUM) are indexed by the processor-independent tags.
      [DT_NUM,DT_NUM+DT_THISPROCNUM) are indexed by the tag minus DT_LOPROC.
      [DT_NUM+DT_THISPROCNUM,DT_NUM+DT_THISPROCNUM+DT_VERSIONTAGNUM) are
      indexed by DT_VERSIONTAGIDX(tagvalue).
      [DT_NUM+DT_THISPROCNUM+DT_VERSIONTAGNUM,
      DT_NUM+DT_THISPROCNUM+DT_VERSIONTAGNUM+DT_EXTRANUM) are indexed by
      DT_EXTRATAGIDX(tagvalue).
      [DT_NUM+DT_THISPROCNUM+DT_VERSIONTAGNUM+DT_EXTRANUM,
      DT_NUM+DT_THISPROCNUM+DT_VERSIONTAGNUM+DT_EXTRANUM+DT_VALNUM) are
      indexed by DT_VALTAGIDX(tagvalue) and
      [DT_NUM+DT_THISPROCNUM+DT_VERSIONTAGNUM+DT_EXTRANUM+DT_VALNUM,
      DT_NUM+DT_THISPROCNUM+DT_VERSIONTAGNUM+DT_EXTRANUM+DT_VALNUM+DT_ADDRNUM)
      are indexed by DT_ADDRTAGIDX(tagvalue), see <elf.h>.  */

   ElfW(Dyn) *l_info[DT_NUM + DT_VERSIONTAGNUM
         + DT_EXTRANUM + DT_VALNUM + DT_ADDRNUM];
   const ElfW(Phdr) *l_phdr;	/* Pointer to program header table in core.  */
   ElfW(Addr) l_entry;		/* Entry point location.  */
   ElfW(Half) l_phnum;		/* Number of program header entries.  */
   ElfW(Half) l_ldnum;	   	/* Number of dynamic segment entries.  */
   /* Start and finish of memory map for this object.  l_map_start
      need not be the same as l_addr.  */
   ElfW(Addr) l_map_start, l_map_end;
   /* End of the executable part of the mapping.  */
   ElfW(Addr) l_text_end;
   /* The plt_begin */
   ElfW(Addr) l_plt_begin;

} dasics_link_map_t;

typedef uint64_t (*fixup_entry_t)(uint64_t, uint64_t);
#define LIB_NUM 256
/*
 * The struct store all the useful message of one module when doing dynamic link
 * most important is the plt, got and the map address
 */
typedef struct umain_got
{
   ElfW(Addr) l_addr;		/* Difference between the address in the ELF
            file and the addresses in memory.  */
   char l_name[256];	      /* Module name, for 256 */
   
   /* real_name */
   char * real_name;

   /* For the list */
   struct umain_got * umain_got_next, *umain_got_prev;

   // fixup handler and arg
   fixup_entry_t fixup_handler;
   dasics_link_map_t *map_link;
   /* 
    * If the lib is a trusted, the _point_got will be the pointed untrusted lib
    * else it will be a point trusted lib (if the lib has a pointed)
    * 
    * Or be NULL
    */
   struct umain_got * _point_got;

   /* The useful message of the module */
   ElfW(Dyn) *l_info[DT_NUM + DT_VERSIONTAGNUM
         + DT_EXTRANUM + DT_VALNUM + DT_ADDRNUM];
   /* The plt begin which will be used to count the got, it can be read from got[2], got[3]... */
   uint64_t *got_begin  ;
   uint64_t *plt_begin  ;
   uint64_t got_num     ;
   uint64_t dynamic     ;		/* Dynamic section of the shared object.  */

   uint64_t _local_got[LIB_NUM]; /* Num of lib call */

   const ElfW(Phdr) *l_phdr;	/* Pointer to program header table in core.  */
   ElfW(Addr) l_entry;		   /* Entry point location.  */
   ElfW(Half) l_phnum;		   /* Number of program header entries.  */
   ElfW(Half) l_ldnum;	   	/* Number of dynamic segment entries.  */
   

   uint64_t _flags;           /* Define in dasics_link_manager */

   mem_struct_t * mem;        /* Mem struct of the library */

   /* record the _text, plt, r_only_area, _w_area(imply read) */
   uint64_t _text_start, _text_end;
   uint64_t _plt_start, _plt_end;
   uint64_t _r_start, _r_end;
   uint64_t _w_start, _w_end; 
   uint64_t _map_start, _map_end;
} umain_got_t;




/* Show for the umain, dasics will search dynamic link ulib function from here */
extern umain_got_t * _umain_got_table;
/* This handler is passed by the dynamic linker, which in the stack auxv
 * This function is address of the "riscv-gnu-toolchain/glibc/elf/dl-runtime.c
 * for :
 * DL_FIXUP_VALUE_TYPE
 * attribute_hidden __attribute ((noinline)) ARCH_FIXUP_ATTRIBUTE
 * _dl_fixup ( 
 *    struct link_map *l, ElfW(Word) reloc_arg)
 * 
 * If the a0 filled with the got[1], a1 filled with thrice of the .got.plt offset, 
 * call dll_fixup_function, it will fill ulib function address in the a0 but not 
 * execute it.
 * 
 * This is used for find the address of ulib function dely binding of dynamic 
 * library
 */
extern fixup_entry_t dll_fixup_handler;
extern fixup_entry_t dll_fixup_handler_lib;

// Trust lib gather
extern char *my_trust_lib[];

extern dasics_link_map_t * link_map_main;
extern dasics_link_map_t * link_map_copy;

#define MAP_MAIN 1
#define MAP_COPY 2
/* create the _umain_got_table */
int create_umain_got_chain(dasics_link_map_t * __map, char * name);
umain_got_t * create_umain_got_chain_copy(dasics_link_map_t * __map);
int perfect_umain_got(umain_got_t * _main_got);

/* 
 * Used to check the copy of trusted lib
 */
int check_copy_library();


/* used for dasics trap */
umain_got_t * _get_trap_area(reg_t uepc);
int _is_plt_area(reg_t uepc, umain_got_t * _got_entry);

/* used for opening maincall for lib func */
int _open_maincall();

/* Found got struct by real_name */
umain_got_t * _find_got(const char * name);

/* Used to find got struct by trusted real_name + .copy */
umain_got_t * _find_got_untrust(umain_got_t * entry, const char * name);

/* This func is used to reloc the target */
uint64_t _call_reloc(umain_got_t *entry, uint64_t target);

/* Get lib_name from lib */
char * _get_lib_name(umain_got_t * entry, uint64_t plt_idx);

/* Print all lib func of the library's name */
void print_all_lib_func();

/* a function before do func exit */
void ready_lib_exit();

#define NOEXEC 1
#define NORMAL_CALL 2
/* used for dasics maincall do dynamic call and return */
int dasics_dynamic_call(umain_got_t * entry, regs_context_t * r_regs);
int dasics_dynamic_return(regs_context_t * r_regs);


// malloc memory on heap simplily
static inline uint64_t __BRK(uint64_t ptr)
{
    register long a7 asm("a7") = 214;
    register long a0 asm("a0") = ptr;
    asm volatile("ecall"                        \
                 : "+r"(a0)                     \
                 : "r"(a7)                      \
                 : "memory");

    return a0;
}

static inline uintptr_t
reloc_offset (uintptr_t plt0, uintptr_t pltn)
{
  return pltn;
}
void open_memory(umain_got_t * _main);

#ifdef DASICS_DEBUG
/* 
 * Debug print the message of the map linker message
 */
static inline void debug_print_link_map(dasics_link_map_t * map)
{
   my_printf("++++++++++++++++  START DEBUG dasics_link_map_t ++++++++++++++++\n");
   my_printf("l_addr: 0x%lx\n", map->l_addr);
   my_printf("l_name: %s\n", map->l_name);
   my_printf("l_ld: 0x%lx\n", map->l_ld);
   
   if (map->l_info[DT_NEEDED])
      my_printf("DT_NEEDED: 0x%lx\n",     D_PTR(map, l_info[DT_NEEDED]));
   if (map->l_info[DT_PLTRELSZ])
      my_printf("DT_PLTRELSZ: 0x%lx\n",   D_PTR(map, l_info[DT_PLTRELSZ]));
   if (map->l_info[DT_PLTGOT])
      my_printf("DT_PLTGOT: 0x%lx\n",     D_PTR(map, l_info[DT_PLTGOT]));
   if (map->l_info[DT_STRTAB])
      my_printf("DT_STRTAB: 0x%lx\n",     D_PTR(map, l_info[DT_STRTAB]));
   if (map->l_info[DT_SYMTAB])
      my_printf("DT_SYMTAB: 0x%lx\n",     D_PTR(map, l_info[DT_SYMTAB]));
   if (map->l_info[DT_RELA])
      my_printf("DT_RELA: 0x%lx\n",       D_PTR(map, l_info[DT_RELA]));
   if (map->l_info[DT_RELASZ])
      my_printf("DT_RELASZ: 0x%lx\n",     D_PTR(map, l_info[DT_RELASZ]));
   if (map->l_info[DT_RELAENT])
      my_printf("DT_RELAENT: 0x%lx\n",    D_PTR(map, l_info[DT_RELAENT]));
   if (map->l_info[DT_SYMENT])
      my_printf("DT_SYMENT: 0x%lx\n",     D_PTR(map, l_info[DT_SYMENT]));
   if (map->l_info[DT_STRSZ])
      my_printf("DT_STRSZ: 0x%lx\n",      D_PTR(map, l_info[DT_STRSZ]));
   if (map->l_info[DT_INIT])
      my_printf("DT_INIT: 0x%lx\n",       D_PTR(map, l_info[DT_INIT]));
   if (map->l_info[DT_FINI])
      my_printf("DT_FINI: 0x%lx\n",       D_PTR(map, l_info[DT_FINI]));
   if (map->l_info[DT_SONAME])
      my_printf("DT_SONAME: 0x%lx\n",     D_PTR(map, l_info[DT_SONAME]));
   if (map->l_info[DT_SYMBOLIC])
      my_printf("DT_SYMBOLIC: 0x%lx\n",   D_PTR(map, l_info[DT_SYMBOLIC]));
   if (map->l_info[DT_REL])
      my_printf("DT_REL: 0x%lx\n",        D_PTR(map, l_info[DT_REL]));
   if (map->l_info[DT_RELSZ])
      my_printf("DT_RELSZ: 0x%lx\n",      D_PTR(map, l_info[DT_RELSZ]));
   if (map->l_info[DT_RELENT])
      my_printf("DT_RELENT: 0x%lx\n",     D_PTR(map, l_info[DT_RELENT]));
   if (map->l_info[DT_DEBUG])
      my_printf("DT_DEBUG: 0x%lx\n",      D_PTR(map, l_info[DT_DEBUG]));
   if (map->l_info[DT_TEXTREL])
      my_printf("DT_TEXTREL: 0x%lx\n",    D_PTR(map, l_info[DT_TEXTREL]));
   if (map->l_info[DT_JMPREL])
      my_printf("DT_JMPREL: 0x%lx\n",     D_PTR(map, l_info[DT_JMPREL]));
   if (map->l_info[DT_BIND_NOW])
      my_printf("DT_BIND_NOW: 0x%lx\n",   D_PTR(map, l_info[DT_BIND_NOW]));
   if (map->l_info[DT_INIT_ARRAY])
      my_printf("DT_INIT_ARRAY: 0x%lx\n", D_PTR(map, l_info[DT_INIT_ARRAY]));
   if (map->l_info[DT_FINI_ARRAY])
      my_printf("DT_FINI_ARRAY: 0x%lx\n", D_PTR(map, l_info[DT_FINI_ARRAY]));
   if (map->l_info[DT_INIT_ARRAYSZ])
      my_printf("DT_INIT_ARRAYSZ: 0x%lx\n", D_PTR(map, l_info[DT_INIT_ARRAYSZ]));
   if (map->l_info[DT_FINI_ARRAYSZ])
      my_printf("DT_FINI_ARRAYSZ: 0x%lx\n", D_PTR(map, l_info[DT_FINI_ARRAYSZ]));
   
   my_printf("l_phdr: 0x%lx\n", map->l_phdr);
   my_printf("l_entry: 0x%lx\n", map->l_entry);
   my_printf("l_phnum: 0x%lx\n", map->l_phnum);
   my_printf("l_ldnum: 0x%lx\n", map->l_ldnum);
   my_printf("l_map_start: 0x%lx, l_map_end: 0x%lx\n", map->l_map_start, map->l_map_end);
   my_printf("l_text_end: 0x%lx\n", map->l_text_end);
   my_printf("l_plt_begin: 0x%lx\n", map->l_plt_begin);

   my_printf("++++++++++++++++   END DEBUG dasics_link_map_t  ++++++++++++++++\n");

}

/* 
 * Debug print the message of the umain_got_t message
 */
static inline debug_print_umain_map(umain_got_t * _umain_map)
{
   my_printf("++++++++++++++++  START DEBUG umain_got_t  ++++++++++++++++\n");
   my_printf("l_addr: 0x%lx\n", _umain_map->l_addr);
   my_printf("l_name: %s\n", _umain_map->l_name);
   my_printf("real_name: %s\n", _umain_map->real_name);


   my_printf("got_begin: 0x%lx\n", _umain_map->got_begin);
   my_printf("plt_begin: 0x%lx\n", _umain_map->plt_begin);
   my_printf("dynamic: 0x%lx\n", _umain_map->dynamic);
   my_printf("got_num: 0x%lx\n", _umain_map->got_num);

   if (_umain_map->got_begin)
   {
      my_printf("runtime-resolve handler: 0x%lx, arg: 0x%lx\n", _umain_map->got_begin[0], _umain_map->got_begin[1]);
   }

   my_printf("l_phdr: 0x%lx\n", _umain_map->l_phdr);
   my_printf("l_entry: 0x%lx\n", _umain_map->l_entry);
   my_printf("l_phnum: 0x%lx\n", _umain_map->l_phnum);

   my_printf("_text_start: 0x%lx, _text_end: 0x%lx\n", _umain_map->_text_start, _umain_map->_text_end);
   my_printf("_plt_start: 0x%lx, _plt_end: 0x%lx\n", _umain_map->_plt_start, _umain_map->_plt_end);
   my_printf("_r_start: 0x%lx, _r_end: 0x%lx\n", _umain_map->_r_start, _umain_map->_r_end);
   my_printf("_w_start: 0x%lx, _w_end: 0x%lx\n", _umain_map->_w_start, _umain_map->_w_end);
   my_printf("_map_start: 0x%lx, _map_end: 0x%lx\n", _umain_map->_map_start, _umain_map->_map_end);


   my_printf("++++++++++++++++   END DEBUG umain_got_t   ++++++++++++++++\n");
}


/* 
 * Debug print the message of Elf_Ehdr message
 */
static inline debug_print_Ehdr(ElfW(Ehdr) * Ehdr)
{
   my_printf("++++++++++++++++  START DEBUG Elf_Ehdr  ++++++++++++++++\n");
   my_printf("Ehdr->e_shentsize: 0x%lx\n", Ehdr->e_shentsize);
   my_printf("Ehdr->e_shnum: 0x%lx\n", Ehdr->e_shnum);
   my_printf("Ehdr->e_shoff: 0x%lx\n", Ehdr->e_shoff);
   my_printf("Ehdr->e_shstrndx: 0x%lx\n", Ehdr->e_shstrndx);

   my_printf("++++++++++++++++   END DEBUG Elf_Ehdr   ++++++++++++++++\n");
}


#endif

#endif