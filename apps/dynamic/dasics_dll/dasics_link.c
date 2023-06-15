#include <dasics_link.h>
#include <stddef.h>
#include <utrap.h>
#include <dasics_link_manager.h>
#include <lib-names.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <elf.h>

umain_got_t * _umain_got_table = NULL;
fixup_entry_t dll_fixup_handler = NULL;
fixup_entry_t dll_fixup_handler_lib = NULL;

dasics_link_map_t * link_map_main = NULL;
dasics_link_map_t * link_map_copy = NULL;
// trust lib 
char *my_trust_lib[] =
{
    LIBANL_SO,                       
    LIBBROKENLOCALE_SO,             
    LIBCRYPT_SO,                   
    LIBC_SO,                         
    LIBDL_SO,                   
    LIBGCC_S_SO,                   
    LIBMVEC_SO,                  
    LIBM_SO,                         
    LIBNSL_SO,                     
    LIBNSS_COMPAT_SO,           
    LIBNSS_DB_SO,                  
    LIBNSS_DNS_SO,                
    LIBNSS_FILES_SO,                 
    LIBNSS_HESIOD_SO,                
    LIBNSS_LDAP_SO,                 
    LIBNSS_NISPLUS_SO,             
    LIBNSS_NIS_SO,                 
    LIBNSS_TEST1_SO,               
    LIBNSS_TEST2_SO,               
    LIBPTHREAD_SO,                  
    LIBRESOLV_SO,                     
    LIBRT_SO,                     
    LIBTHREAD_DB_SO,             
    LIBUTIL_SO,               
    NULL
};



/*
 * This function is used to do memcpy in the _dasics_entry_stage2 to avoid
 * to use glibic memecpy
 */
static inline int __dasics_linker_memcpy(char *dest, const char *src, unsigned int len)
{
    for (int i = 0; i < len; i++)
    {
        dest[i] = src[i];
    }
    return len;
}

static inline int __dasics_linker_strcpy(char *dest, const char *src)
{
    char *tmp = dest;

    while (*src) {
        *dest++ = *src++;
    }

    *dest = '\0';

    return tmp;
}

static inline int __dasics_linker_strcmp(const char *str1, const char *str2)
{
    while (*str1 && *str2) {
        if (*str1 != *str2) {
            return (*str1) - (*str2);
        }
        ++str1;
        ++str2;
    }
    return (*str1) - (*str2);
}

static char * get_real_name(char * name)
{
    char * local_name = NULL;
    for (int i = 0; name[i] != '\0'; i++)
    {
        if (name[i] == '/')
            local_name = &name[i + 1];       
    }
    if (local_name == NULL)
        local_name = name;    
    return local_name;
}

static int _is_trust(char * name)
{
    if (!__dasics_linker_strcmp(get_real_name(name), LIBC_SO))
        return 1;
    return 0;
}

static void fill_got_by_link(umain_got_t *_got, dasics_link_map_t * _map)
{
    /* copy l_info */
    for (int _j = 0; _j < DT_NUM + DT_VERSIONTAGNUM
        + DT_EXTRANUM + DT_VALNUM + DT_ADDRNUM; _j++)
    {
        _got->l_info[_j] = _map->l_info[_j];
    }
    
    /* 
        * If _map->l_info[DT_PLTGOT] is't NULL which means 
        * that there will exist .got Section, we will calculate 
        * the "_dl_runtime_resolve" handler which saved in the got[0] 
        * and the parameter "link_map" which saved in the got[1].
        * 
        * in the meantime, the got[2 ~ last one) saved the plt[0], but the 
        * got[last one] saved the dynamic's section' addr, by doing this
        * , we get the got_begin, plt_begin and dynamic begin
        * 
        */
    if (_map->l_info[DT_PLTGOT])
    {
        /* copy l_info */
        _got->got_begin = (uint64_t *)(D_PTR(_map, l_info[DT_PLTGOT]) + \
                                            _map->l_addr);
        /* get the object's plt_begin from the DT_PLTGOT[2], 
        the 0 and 1 will be reserved, we add l_addr */
        _got->plt_begin = (uint64_t *)(_map->l_plt_begin + _map->l_addr);
        uint64_t * _map_got_begin = _got->got_begin;  
        
        /* Get the dynamic addr from the ElfW(Dyn) *l_ld, 
            * actually, the last item of the .got table stores
            * the addr of the dynamic .section
            */
        _got->dynamic = (uint64_t)_map->l_ld; 
        
        /* the map_start, end */
        _got->_map_start = _map->l_map_start;
        _got->_map_end = _map->l_map_end;

        /* Now, we will count the number of got table items */
        _got->got_num = 0;

        /*
        * Actually, the last item of the got equal the dynamic section 
        * addr in the memeory, so we can cauculate the number of the got.
        */
        int _j = 2;
        uint64_t judge_dynamic = _got->dynamic - _map->l_addr;
        for (_j = 2; _map_got_begin[_j] !=  judge_dynamic; _j++)
            _got->got_num++;

    } else 
    {
        _got->got_begin = NULL;
        _got->plt_begin = NULL;
        _got->dynamic = NULL;
    }


    _got->l_phdr = _map->l_phdr;
    _got->l_entry = _map->l_entry;
    _got->l_phnum = _map->l_phnum;    
}



extern uint64_t _interp_start[];
/*
 * This fuinction is used to init umain dasics_link_map
 * include the exe, linker, all library
 */
int create_umain_got_chain(dasics_link_map_t * __map, char * name)
{
    /* align the heap for 128bit */
    _umain_got_table = (umain_got_t *)__BRK(ROUND(__BRK(NULL), 0x16UL));

    /* count the number of the library */
    dasics_link_map_t * _map_init = __map;
    int map_num = 0;
    int real_num = 0;
    while (__map)
    { 
        if (_map_init->l_name != NULL &&\
             !__dasics_linker_strcmp(get_real_name(__map->l_name), LINUX_VDSO))
        {
            __map = __map->l_next;
            map_num++; 
            continue;
        }
        __map = __map->l_next;    
        map_num++;
        real_num++;
    }

    /* alloc a place on the heap */
    __BRK((uint64_t)_umain_got_table + sizeof(umain_got_t)*real_num);

    _umain_got_table->umain_got_next = _umain_got_table->umain_got_prev = NULL;

    /* now, we have alloc a place for all the library, we will init it */
    for (int _i = 0; _i < real_num; _i++)
    {
        if (_map_init->l_name != NULL &&\
             !__dasics_linker_strcmp(get_real_name(_map_init->l_name), LINUX_VDSO))
        {
            _map_init = _map_init->l_next; 
        }     
        _umain_got_table[_i]._flags = 0; 
        if (!__dasics_linker_strcmp(_map_init->l_name, (char *)_interp_start))
        {
            // _map_init = _map_init->l_next;
            _umain_got_table[_i]._flags |= (LINK_AREA | MAIN_AREA);
        } 
        else if (_i)
        {
            _umain_got_table[_i]._flags |= LIB_AREA;
        } 
        /* l_addr */
        _umain_got_table[_i].l_addr = _map_init->l_addr;
        /* copy name */ 
        if (_i)
            __dasics_linker_strcpy(_umain_got_table[_i].l_name, _map_init->l_name);

        /* fill the item */
        fill_got_by_link(&_umain_got_table[_i], _map_init);

        /* Create chain list for management */
        if (_i)
        {
            _umain_got_table[_i - 1].umain_got_next = &_umain_got_table[_i];
            _umain_got_table[_i].umain_got_prev = &_umain_got_table[_i - 1];
            _umain_got_table[_i].umain_got_next = NULL;
        } else 
        {
            /* ELF_AREA */
            _umain_got_table[_i]._flags |= (ELF_AREA | MAIN_AREA);
            _umain_got_table[_i].umain_got_next = _umain_got_table[_i].umain_got_prev = NULL;
            __dasics_linker_strcpy(_umain_got_table[_i].l_name, name);
        }
        if (_is_trust(_umain_got_table[_i].l_name))
            _umain_got_table[_i]._flags |= MAIN_AREA;

        _umain_got_table->_point_got = NULL;
        _umain_got_table[_i].real_name = get_real_name(_umain_got_table[_i].l_name);
        
        // fiixup
        _umain_got_table[_i].fixup_handler = (fixup_entry_t)dll_fixup_handler;
        _umain_got_table[_i].mem = NULL;
        if (_umain_got_table[_i].got_begin)
            _umain_got_table[_i].map_link = (dasics_link_map_t *)_umain_got_table[_i].got_begin[1];
        /* find next dasics_link_map_t */
        _map_init = _map_init->l_next;
    }
    // fill other data
    perfect_umain_got(_umain_got_table);
}

/*
 * This function is used to create the copy
 */
umain_got_t * create_umain_got_chain_copy(dasics_link_map_t * __map)
{
    /* align the heap for 128bit */
    umain_got_t * _local_got_table = (umain_got_t *)__BRK(ROUND(__BRK(NULL), 0x16UL));

    /* count the number of the library */
    dasics_link_map_t * _map_init = __map;
    int map_num = 0;
    int real_num = 0;
    while (__map)
    { 
        if (!(__map->l_addr) || !__dasics_linker_strcmp(get_real_name(__map->l_name), LINUX_VDSO))
        {
            __map = __map->l_next;
            map_num++; 
            continue;
        }
        __map = __map->l_next;    
        map_num++;
        real_num++;
    }
    /* alloc a place on the heap */
    __BRK((uint64_t)_local_got_table + sizeof(umain_got_t)*real_num);
    _local_got_table->umain_got_next = _local_got_table->umain_got_prev = NULL;

    /* now, we have alloc a place for all the library, we will init it */
    for (int _i = 0; _i < real_num; _i++)
    {
        // jump exe and the linux-vdso.so.1
        while (!_map_init->l_addr || \
            !__dasics_linker_strcmp(get_real_name(_map_init->l_name), LINUX_VDSO))
        {
            _map_init = _map_init->l_next; 
        }     
        _local_got_table[_i]._flags = 0; 
        if (!__dasics_linker_strcmp(_map_init->l_name, (char *)_interp_start))
        {
            // _map_init = _map_init->l_next;
            _local_got_table[_i]._flags |= (LINK_AREA | MAIN_AREA);
        } 
        else
        {
            _local_got_table[_i]._flags |= LIB_AREA;
        } 
        /* l_addr */
        _local_got_table[_i].l_addr = _map_init->l_addr;

        __dasics_linker_strcpy(_local_got_table[_i].l_name, _map_init->l_name);

        /* fill the item */
        fill_got_by_link(&_local_got_table[_i], _map_init);

        /* Create chain list for management */
        if (_i)
        {
            _local_got_table[_i - 1].umain_got_next = &_local_got_table[_i];
            _local_got_table[_i].umain_got_prev = &_local_got_table[_i - 1];
            _local_got_table[_i].umain_got_next = NULL;
        } else 
        {
            _local_got_table[_i].umain_got_next = _local_got_table[_i].umain_got_prev = NULL;
        }

        _local_got_table->_point_got = NULL;
        _local_got_table[_i].real_name = get_real_name(_local_got_table[_i].l_name);

        _local_got_table[_i].fixup_handler = (fixup_entry_t)dll_fixup_handler_lib;
        _umain_got_table[_i].mem = NULL;
        if (_local_got_table[_i].got_begin)
            _local_got_table[_i].map_link = (dasics_link_map_t *)_local_got_table[_i].got_begin[1];
        /* find next dasics_link_map_t */
        _map_init = _map_init->l_next;
    }
    // fill other data
    perfect_umain_got(_local_got_table);    
    return _local_got_table;
}



/* 
 * This function is used to fill the _text_begin, end; _plt_start,end;
 * _r_begin,end; _w_begin, end;
 */
int perfect_umain_got(umain_got_t * _main_got)
{
    umain_got_t * _local_umain_got = _main_got;
    // Traverse the chain
    while (_local_umain_got)
    {
        // fill the _plt_start, end
        if ((uint64_t)_local_umain_got->plt_begin)
        {
            _local_umain_got->_plt_start = (uint64_t)_local_umain_got->plt_begin + 0x20UL;
            _local_umain_got->_plt_end = _local_umain_got->_plt_start + _local_umain_got->got_num * 0x10UL;           
        }
 
        // fill _r_begin, end; _w_start, end;
        ElfW(Phdr) * _local_Phdr = (ElfW(Phdr) *)_local_umain_got->l_phdr;
        int _set_r = 0;
        int _set_w = 0;
        int _set_x = 0;
        _local_umain_got->_r_start = _local_umain_got->_r_end = 0;
        _local_umain_got->_w_start = _local_umain_got->_w_end = 0;
        // Circulate the PT_LOAD Phdr
        for (int _i = 0; _i < _local_umain_got->l_phnum; _i++)
        {
            #define ADD_TWO(X, Y)((X) + (Y))
            #define ADD_THREE(X, Y, Z)((X) + (Y) + (Z))
            if (_local_Phdr[_i].p_type == PT_LOAD)
            {
                // Only set for the executable
                if (!_set_x && \
                    (_local_Phdr[_i].p_flags & PF_X))
                {
                    _set_x = 1;
                    _local_umain_got->_text_start = ADD_TWO(_local_Phdr[_i].p_vaddr, \
                                                            _local_umain_got->l_addr);
                }

                if ((_local_Phdr[_i].p_flags & PF_X))
                {
                    _local_umain_got->_text_end = ADD_THREE(_local_Phdr[_i].p_vaddr,  \
                                                  _local_Phdr[_i].p_memsz, \
                                                  _local_umain_got->l_addr);
                }                

                // Only set for the READ_ONLY type Phdr 
                if (!_set_r && \
                   (_local_Phdr[_i].p_flags & PF_R) && \
                   !(_local_Phdr[_i].p_flags & PF_W)
                   )
                {
                    _set_r = 1;
                    _local_umain_got->_r_start = ADD_TWO(_local_Phdr[_i].p_vaddr, \
                                                         _local_umain_got->l_addr);
                } 
                    
                if ((_local_Phdr[_i].p_flags & PF_R) && \
                   !(_local_Phdr[_i].p_flags & PF_W)
                   )
                {
                    _local_umain_got->_r_end = ADD_THREE(_local_Phdr[_i].p_vaddr,  \
                                                  _local_Phdr[_i].p_memsz, \
                                                  _local_umain_got->l_addr);
                } 

                // Only set for the WRITE_ONLY type Phdr
                if (!_set_w && \
                   (_local_Phdr[_i].p_flags & PF_R) && \
                   (_local_Phdr[_i].p_flags & PF_W)
                   )
                {
                    _set_w = 1;
                    _local_umain_got->_w_start = ADD_TWO(_local_Phdr[_i].p_vaddr, \
                                                         _local_umain_got->l_addr);
                } 
                
                if ((_local_Phdr[_i].p_flags & PF_R) && \
                    (_local_Phdr[_i].p_flags & PF_W)
                   )
                {
                    _local_umain_got->_w_end = ADD_THREE(_local_Phdr[_i].p_vaddr,  \
                                                  _local_Phdr[_i].p_memsz, \
                                                  _local_umain_got->l_addr);
                } 
            }
        }
        _local_umain_got = _local_umain_got->umain_got_next;
    }
}

// check whether the lib func have a exit func
static int have_exit(umain_got_t *entry)
{
    if (entry->l_info[DT_INIT] != NULL)
    {
        /* copy addr from trusted lib */
        
        if (*(uint64_t *)(entry->l_addr + entry->l_info[DT_INIT]->d_un.d_ptr) != NULL)
            return 1;
    }

    
    ElfW(Dyn) *untrusted_init_array = entry->l_info[DT_INIT_ARRAY];

    if (untrusted_init_array != NULL)
    {
        unsigned int j;
        unsigned int jm;
        ElfW(Addr) *addrs;

        jm = entry->l_info[DT_INIT_ARRAYSZ]->d_un.d_val / sizeof (ElfW(Addr));


        addrs = (ElfW(Addr) *) (untrusted_init_array->d_un.d_ptr + entry->l_addr);
        for (j = 0; j < jm; ++j)
	        if (addrs[j] != NULL)
                return 1;
    }    
    return 0;
}


/*
 * This function is used to open the lib which have fini func
 */
void ready_lib_exit()
{
#ifdef DASICS_DEBUG    
    my_printf("[DEBUG] ready for exit enviroment\n");
#endif
    umain_got_t * _local = _umain_got_table;

    while (_local)
    {
        if (!(_local->_flags & MAIN_AREA) && \
                have_exit(_local))
        {

            // only open valid and free zone
            dasics_libcfg_alloc(DASICS_LIBCFG_X | DASICS_LIBCFG_V , \
                            _local->_text_end, _local->_plt_start);
        }
        _local = _local->umain_got_next;
    }
}

/*
 * This function is used to find out the trap area
 */
umain_got_t * _get_trap_area(reg_t uepc)
{
    umain_got_t * _got_entry = _umain_got_table;
    
    while (_got_entry)
    {
        if (RANGE(uepc, \
                _got_entry->_text_start, \
                _got_entry->_text_end)
            )
            return _got_entry;
        _got_entry = _got_entry->umain_got_next;
    }
    
    return NULL;
}

/*
 * This function is used to figure out which plt[x] the uepc was seted,
 * if find, return the idx or retuen -1;
 *
 */
int _is_plt_area(reg_t uepc, umain_got_t * _got_entry)
{
    if (RANGE(uepc, \
             _got_entry->_plt_start, \
             _got_entry->_plt_end))
    {
        return (uepc - _got_entry->_plt_start) / 0x10UL;
    }

    return NOEXIST;
}

/*
 * This func is used to find the struct umain_got_t by the real
 * name
 */
umain_got_t * _find_got(const char * name)
{
    umain_got_t *_got = _umain_got_table; 

    while (_got)
    {
        if (!strcmp(name, _got->real_name))
            return _got;

        _got = _got->umain_got_next;
    }
    return NULL;
}

/*
 * This func is used to find the trusted library's untrusted copy by the real
 * name 
 * 
 * @return umain_got_t
 */
umain_got_t * _find_got_untrust(umain_got_t * entry, const char * name)
{
    umain_got_t *_got = _umain_got_table; 

    while (_got)
    {
        if (!strcmp(name, _got->real_name) && _got != entry)
            return _got;

        _got = _got->umain_got_next;
    }
    return NULL;
}

/**
 * This function is used to redirect func's addr
 */
uint64_t _call_reloc(umain_got_t *entry, uint64_t target)
{
    /* Get the target area of the target addr */
    umain_got_t * _target_got = _get_trap_area(target);
    if (_target_got == NULL)
    {
        my_printf("[ERROR] DASICS error! target addr error: 0x%lx!\n", target);
        exit(1);
    }

    /* The trusted area want to call trusted func */
    if ((entry->_flags & MAIN_AREA) &&
         (_target_got->_flags & MAIN_AREA))
    {
        return target;
    }
        
    /* The trusted area want to call untrusted func */
    if ((entry->_flags & MAIN_AREA) &&
         !(_target_got->_flags & MAIN_AREA))
    {
        if (_target_got->_point_got != NULL)
        {
            #ifdef DASICS_DEBUG
            // my_printf("reloc: change 1 to: 0x%lx\n", \ 
            // (target - _target_got->l_addr) + _target_got->_point_got->l_addr);
            #endif
            return (target - _target_got->l_addr) + _target_got->_point_got->l_addr;
        }
    }

    /* The untrusted area want to call trusted func */
    if (!(entry->_flags & MAIN_AREA) &&
         (_target_got->_flags & MAIN_AREA))
    {
        if (_target_got->_point_got != NULL)
        {
            #ifdef DASICS_DEBUG
            // my_printf("reloc: change 2 to: 0x%lx\n", \
            // (target - _target_got->l_addr) + _target_got->_point_got->l_addr);
            #endif
            return (target - _target_got->l_addr) +  _target_got->_point_got->l_addr;
        }
    }    

    return target;
}

/*
 * This function is used to get the library's func name by the index
 */
char * _get_lib_name(umain_got_t * entry, uint64_t plt_idx)
{

    ElfW(Word) reloc_arg = (((uint64_t)plt_idx * 0x10UL) >> 1) * 3;
    const ElfW(Sym) *const symtab
        = (const void *) (D_PTR (entry, l_info[DT_SYMTAB]) + entry->l_addr);
    const char *strtab = (const void *) (D_PTR (entry, l_info[DT_STRTAB]) + entry->l_addr);

    const uintptr_t pltgot = (uintptr_t) (D_PTR (entry, l_info[DT_PLTGOT]));   

    const PLTREL *const reloc
        = (const void *) ((D_PTR (entry, l_info[DT_JMPREL]) + entry->l_addr)
                + reloc_offset (pltgot, reloc_arg));    
    const ElfW(Sym) *sym = &symtab[ELFW(R_SYM) (reloc->r_info)];

    return strtab + sym->st_name;
}


/*
 * This function is used to print all excternal func's name of all library
 * will call
 */
void print_all_lib_func()
{
    umain_got_t * local_got = _umain_got_table;

    while (local_got)
    {
        for (int i = 0; i < local_got->got_num; i++)
        {
            my_printf("Lib %s want to call func: %s\n", local_got->real_name, \
                            _get_lib_name(local_got, i));
        }
        local_got = local_got->umain_got_next;
    }
    

}

/*
 * This function is used to make the PF_W be writable for stage 3
 */
void open_memory(umain_got_t * _main)
{
    ElfW(Phdr) * _Phdr = (ElfW(Phdr) *)_main->l_phdr;
    // Circulate the PT_LOAD Phdr
    for (int _i = 0; _i < _main->l_phnum; _i++)
    {
        if (_Phdr[_i].p_type == PT_LOAD)
            if (_Phdr[_i].p_flags & PF_W)
            {
                mprotect(ROUNDDOWN(_Phdr[_i].p_vaddr, PAGE_SIZE), 
                        ROUND(_Phdr[_i].p_vaddr + _Phdr[_i].p_memsz, PAGE_SIZE) \
                        - ROUNDDOWN(_Phdr[_i].p_vaddr, PAGE_SIZE),
                        PROT_READ | PROT_WRITE
                        );
            }
    }
}


