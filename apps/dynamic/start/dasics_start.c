#include <stdio.h>
#include <stdint.h>
#include <uattr.h>
#include <udasics.h>
#include <dasics_start.h>
#include <my_stdio.h>
#include <utrap.h>
#include <asm/dasics_ucontext.h>
#include <dasics_link.h>
#include <sys/mman.h>
#include <cross.h>
#include <mem.h>
#include <dasics_ecall.h>

#define AT_LINKER 55
#define AT_FIXUP 56
#define AT_DASICS 57
#define AT_LINKER_COPY 58
#define AT_TRUST_BASE 59
#define AT_ENTRY 9

// the stage 2's fini pointer, this will be used atexit to add
typedef void (*rtld_fini) (void);

rtld_fini dll_fini;


#define RESET_ENTRY(sp, entry)({       \
            asm volatile ("mv sp , %0" : "+r"(sp)); \
            asm volatile ("mv ra , %0" : "+r"(entry));          \
            asm volatile ("mv gp , zero");          \
            asm volatile ("mv tp , zero");          \
            asm volatile ("mv t0 , zero");          \
            asm volatile ("mv t1 , zero");          \
            asm volatile ("mv t2 , zero");          \
            asm volatile ("mv t3 , zero");          \
            asm volatile ("mv s0 , zero");          \
            asm volatile ("mv s1 , zero");          \
            asm volatile ("mv a0 , zero");          \
            asm volatile ("mv a1 , zero");          \
            asm volatile ("mv a2 , zero");          \
            asm volatile ("mv a3 , zero");          \
            asm volatile ("mv a4 , zero");          \
            asm volatile ("mv a5 , zero");          \
            asm volatile ("mv a6 , zero");          \
            asm volatile ("mv a7 , zero");          \
            asm volatile ("mv s2 , zero");          \
            asm volatile ("mv s3 , zero");          \
            asm volatile ("mv s4 , zero");          \
            asm volatile ("mv s5 , zero");          \
            asm volatile ("mv s6 , zero");          \
            asm volatile ("mv s7 , zero");          \
            asm volatile ("mv s8 , zero");          \
            asm volatile ("mv s9 , zero");          \
            asm volatile ("mv s10 , zero");          \
            asm volatile ("mv s11 , zero");          \
            asm volatile ("mv t3 , zero");          \
            asm volatile ("mv t4 , zero");          \
            asm volatile ("mv t5 , zero");          \
            asm volatile ("mv t6 , zero");          \
            asm volatile ("ret");          \
        })

/*
 * This macro definition is used to get the argc ,argv, envp and auxp easily.
 */
#define DL_FIND_ARG_COMPONENTS(cookie, argc, argv, envp, auxp)	\
  do {									      \
    void **_tmp;							      \
    (argc) = *(long int *) cookie;					      \
    (argv) = (char **) ((long int *) cookie + 1);			      \
    (envp) = (argv) + (argc) + 1;					      \
    for (_tmp = (void **) (envp); *_tmp; ++_tmp)			      \
      continue;								      \
    (auxp) = (void *) ++_tmp;						      \
  } while (0)

typedef struct auxv
{
    /* data */
    uint64_t value[2];
}auxv_t;

extern uint64_t _start[];

/* get the auxv addr */
static uint64_t * _get_auxv(uint64_t *sp)
{
    /* skip argc */
    sp ++;

    /* skip argv[] */
    while (*sp)
        sp ++;

    sp ++;

    /* skip envc[] */
    while (*sp)
        sp ++;

    sp ++;

    return sp;
}


/* get elf entry from the auxv */
static uint64_t _get_auxv_entry(uint64_t sp, uint64_t at_id)
{
    auxv_t * auxv = (auxv_t *)_get_auxv((uint64_t *)sp);
    uint64_t elf_entry = 0;

    while (auxv->value[0])
    {
        if (auxv->value[0] == at_id)
        {
            // my_printf("[info]: Found entry\n");
            elf_entry = auxv->value[1];
        }         
        auxv ++;
    }
    return elf_entry;

}

/* set elf entry from the auxv */
static void _set_auxv_entry(uint64_t sp, uint64_t at_id, uint64_t num)
{
    auxv_t * auxv = (auxv_t *)_get_auxv((uint64_t *)sp);

    while (auxv->value[0])
    {
        if (auxv->value[0] == at_id)
        {
            auxv->value[1] = num;
        }        
        auxv ++;
    }
    
}


/* 
 * if we get the _dll_linker from the auxv which means that 
 * the program need a dynamic linker for stage one, we will 
 * set a utrap function for the dynamic linker to execute 
 * the init function of the library.
 * 
 * or we will go to stage two directly
 */
#define TASK_SIZE 0X4000000000
void _dasics_entry_stage1(uint64_t sp)
{
    my_printf("> [INIT] _dasics_entry_stage1\n");
    uint64_t _dll_linker = _get_auxv_entry((uint64_t *)sp, AT_LINKER);
#ifdef DASICS_DEBUG
    my_printf("> [INIT] linker entry:%lx\n", _dll_linker);
#endif

    init_syscall_check();

    my_printf("> [INIT] Init syscall_check_table successfully\n");
    if (_dll_linker && _get_auxv_entry((uint64_t *)sp, AT_DASICS))
    {
        // change the elf_enrtry to  _umain_entry
        _set_auxv_entry(sp, AT_ENTRY, (uint64_t)_umain_entry);
        #ifdef DASICS_LINUX
        /* 
         * give all lib area be VALID, READ, WRITE, FREE
         * This is not safe
         */
        dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R | DASICS_LIBCFG_W | DASICS_LIBCFG_X, \
                    TASK_SIZE, \
                    TASK_SIZE/2);
        #endif
        // set a trap entry for link time
        csr_write(0x005, (uint64_t)__dasics_start_ufault_entry);
        // Transfer executive authority to dynamic linker
        RESET_ENTRY(sp, _dll_linker);
    }
}

/*
 * for the stage two, we will set a utrap function 
 * for the stage3, and some prepare
 */
void _dasics_entry_stage2(uint64_t sp, rtld_fini fini)
{
#ifdef DASICS_LINUX
    csr_write(0x005, (uint64_t)dasics_ufault_entry);
#endif

    extern uint64_t _got_start[];
    // Get FIXUP
    dll_fixup_handler = (fixup_entry_t)_get_auxv_entry((uint64_t *)sp, AT_FIXUP);
    dll_fini = fini;

    my_printf("> [INIT] _dasics_entry_stage2\n");

#ifdef DASICS_LINUX
    for (int i = 0; i < 2 * DASICS_LIBCFG_WIDTH; i++)
    {
        // clear all dasics lib bounds which used on the dynamic link time
        dasics_libcfg_free(i);
    }
#endif

    dasics_link_map_t * main_map = (dasics_link_map_t *)(_got_start[1]);
    link_map_main = main_map;

#ifdef DASICS_DEBUG
    dasics_link_map_t * debug_main_map = main_map;
    my_printf("> [INIT] linker_runtime_solve_entry: 0x%lx\n", _got_start[0]);
    my_printf("> [INIT] main link_map: 0x%lx\n", _got_start[1]);

    while (debug_main_map)
    {
        debug_print_link_map(debug_main_map);
        my_printf("\n\n");
        my_printf("link_map_address: 0x%lx\n", debug_main_map);
        debug_main_map = debug_main_map->l_next;
    }
#endif

    /* create got chain to support dynamic call  */
    create_umain_got_chain(main_map, *(char**)(sp + 8));


    // print_all_lib_func();
#ifdef DASICS_LINUX
    /* open maincall for dynamic if you need, or we will used ufault exception */
    _open_maincall();
    /* open dynamic's got read jurisdiction */
    dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R, \
                        (uint64_t)_umain_got_table->got_begin + sizeof(uint64_t) * (_umain_got_table->got_num + 2), \
                        (uint64_t)_umain_got_table->got_begin);
    dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R | DASICS_LIBCFG_W | DASICS_LIBCFG_X, \
                        TASK_SIZE, \
                        TASK_SIZE/2);
    my_printf("> [INIT] Init maincall for dynamic successfully\n");
#endif

#ifdef DASICS_DEBUG

    umain_got_t * _local_umain_map = _umain_got_table;
    while (_local_umain_map)
    {
        debug_print_umain_map(_local_umain_map);
        my_printf("\n\n");
        _local_umain_map = _local_umain_map->umain_got_next;
    }

#endif


#ifdef DASICS_COPY
    /* begin to init copy of the trust lib */
    uint64_t copy_linker_dll = _get_auxv_entry(sp, AT_LINKER_COPY);
    my_printf("> [INIT] AT_LINKER_COPY: 0x%lx\n", copy_linker_dll);

    // Open data segment to read
    open_memory(_umain_got_table);

    /* change dasics_flag to 2 let linker just map trust lib on untrusted area*/
    _set_auxv_entry(sp, AT_DASICS, 2);

    /* Go to stage 3 */
    _set_auxv_entry(sp, AT_ENTRY, (uint64_t)_copy_lib_entry);

    csr_write(0x005, (uint64_t)__dasics_start_ufault_entry);
    RESET_ENTRY(sp, copy_linker_dll);

#endif

#ifdef DASICS_LINUX

    check_copy_library();

#endif
}

/*
 * For the stage3, we will do more things for the untrused copy of trusted 
 * library, and then go to _start
 */
void _dasics_entry_stage3(uint64_t sp, rtld_fini fini)
{
    my_printf("> [INIT] _dasics_entry_stage3\n");

#ifdef DASICS_LINUX
    csr_write(0x005, (uint64_t)dasics_ufault_entry);
#endif
    extern uint64_t _got_start[];
    // lib
    dll_fixup_handler_lib = (fixup_entry_t)_get_auxv_entry((uint64_t *)sp, AT_FIXUP);
    link_map_copy = (dasics_link_map_t *)(_got_start[1]);

    umain_got_t * tmp_umain = _umain_got_table;

    /* map untrusted area */
    umain_got_t * _copy_umain_got = create_umain_got_chain_copy(link_map_copy);
    my_printf("> [INIT] Finish create copy chain of untrusted lib successfully\n");

    /* Add untrusted chain to _umain_got_table */
    umain_got_t * local_umain_got = _umain_got_table;
    while (local_umain_got)
    {
        if (local_umain_got->umain_got_next == NULL)
            break;
        local_umain_got = local_umain_got->umain_got_next;
    }

    local_umain_got->umain_got_next = _copy_umain_got;
    _copy_umain_got->umain_got_prev = local_umain_got;
    my_printf("> [INIT] Add copy chain to _umain_got_table successfully\n");
    

#ifdef DASICS_DEBUG
    umain_got_t * _local_umain_map = _umain_got_table;
    while (_local_umain_map)
    {
        debug_print_umain_map(_local_umain_map);
        my_printf("\n\n");
        _local_umain_map = _local_umain_map->umain_got_next;
    }
#endif
    /* set copy lib's GOT to dasics_umain_call */
    _open_maincall();

    /* add the copy lib for the trusted lib in trust area */
    check_copy_library();

    /* Add copy ld.so to atexit */
    atexit(fini);
    my_printf("> [INIT] Add func 0x%lx to exit chain\n", fini);

#ifdef DASICS_LINUX
    for (int i = 0; i < 2 * DASICS_LIBCFG_WIDTH; i++)
    {
        // clear all dasics lib bounds which used on the dynamic link time
        dasics_libcfg_free(i);
    }
    /* open dynamic's got read jurisdiction */
    dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R, \
                        (uint64_t)_umain_got_table->got_begin + sizeof(uint64_t) * (_umain_got_table->got_num + 2), \
                        (uint64_t)_umain_got_table->got_begin);
    
    /* open executable file's plt be free zone */
    dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_X, \
                        _umain_got_table->_plt_end, \
                        _umain_got_table->_plt_start);

    dasics_libcfg_alloc(DASICS_LIBCFG_V | DASICS_LIBCFG_R | DASICS_LIBCFG_W, \
                        TASK_SIZE, \
                        // __BRK(NULL));
                        TASK_SIZE/2);
#endif
    
#ifdef DASICS_COPY && DASICS_LINUX
    init_libcfg_stack();
    my_printf("> [INIT] Init jurisdiction_stack: 0x%lx successfully\n", jurisdiction_stack);
#endif

#ifdef DASICS_LINUX
    init_mm_struct();
    my_printf("> [INIT] Init mem struct successfully\n");
    my_printf("> [INIT] goto start successfully\n");
#endif

}

void print_and_lock()
{

    my_printf("fatal die\n");
    while(1);
}

/*
 * This ufault entry is used to deal the initialization of the untrusted 
 * library, we have make TASK_SIZE/2 ~ TASK_SIZE be free jump region, so 
 * the ufeatch fault will only caused by dasicsReturnPC != ulib func return 
 * main func address
 */
void _dasics_start_ufault_entry(regs_context_t *regs)
{

#ifdef DASICS_DEBUG
    my_printf("[info]: handle one exception when dynamic linker\n");
    my_printf("[info]: ufault occurs, ucause = 0x%lx, uepc = 0x%lx, utval = 0x%lx\n", regs->ucause, regs->uepc, regs->utval);
    my_printf("[info]: dasics_return_pc:0x%lx\n", regs->dasicsReturnPC);	
#endif

    int idx = 0;
    if (regs->ucause == 0x18UL) 
        regs->dasicsReturnPC = regs->utval;
    if (regs->ucause == 0x1aUL)
    {
    #ifdef DASICS_DEBUG
        my_printf("> READ\n");
    #endif
        idx = trap_libcfg_alloc(regs, DASICS_LIBCFG_R, ROUND(regs->utval, 2 * PAGE_SIZE), ROUNDDOWN(regs->utval, 2 * PAGE_SIZE));
        if (idx == -1)
        {
            my_printf("no more libbounds!!\n");
            print_and_lock();
        }
    }
    if (regs->ucause == 0x1cUL)
    {
    #ifdef DASICS_DEBUG
        my_printf("> WRITE\n");
    #endif
        idx = trap_libcfg_alloc(regs, DASICS_LIBCFG_W, ROUND(regs->utval, 2 * PAGE_SIZE), ROUNDDOWN(regs->utval, 2 * PAGE_SIZE));
        if (idx == -1)
        {
            my_printf("no more libbounds!!\n");
            print_and_lock();
        }
    }
#ifdef DASICS_DEBUG 
    my_printf("[info]: dasics_return_pc:0x%lx\n", regs->dasicsReturnPC);
#endif
    return;
}
