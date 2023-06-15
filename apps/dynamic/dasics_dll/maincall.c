#include <dasics_link.h>
#include <stdint.h>
#include <stddef.h>
#include <udasics.h>
#include <my_stdio.h>
#include <dasics_link_manager.h>
#include <cross.h>


extern uint64_t umaincall_helper;

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

/* This function is used to fill dasics_umaincall's addr to 
 * all lib's got table, the got's number is not more than LIB_NUM
 * which can be extend
 */
int _open_maincall()
{
    /* set dasics_umaincall */
    csr_write(0x8a3, (uint64_t)dasics_umaincall);
    umaincall_helper = (uint64_t)dasics_umaincall_helper;
    umain_got_t * _local_main = _umain_got_table;
    
    while (_local_main != NULL)
    {
        if (_local_main->got_num > LIB_NUM - 2)
        {
            my_printf("[Warning]: the %s's got num more than %d \n", 
                    _local_main->real_name, LIB_NUM); 
            while(1);
        }
        /* Local will be filled plt_begin to judge the call times */
        for (int _i = 0; _i < LIB_NUM; _i++)
        {
            _local_main->_local_got[_i] = (uint64_t)_local_main->plt_begin;
        }

        /* got will be filled dasics_umaincall */
        for (int _i = 2; _i < _local_main->got_num + 2; _i++)
        {
            if (_local_main->got_begin[_i] != (uint64_t)_local_main->plt_begin &&
                _local_main->got_begin[_i] != (uint64_t)dasics_umaincall)
                 _local_main->_local_got[_i] = _local_main->got_begin[_i];
            
            _local_main->got_begin[_i] = (uint64_t)dasics_umaincall;
        }
        _local_main = _local_main->umain_got_next;
    }
    
    return 0;

}

/* This function is used to do dasics dynamic function call */
int dasics_dynamic_call(umain_got_t * entry, regs_context_t * r_regs)
{
    // idx of plt
    int plt_idx = NOEXIST;
    umain_got_t * target_got_entry = NULL;
    uint64_t target  = NULL;

    // Failed to find the plt based the t1 register
    if ((plt_idx = \
            _is_plt_area(r_regs->t1, entry)) == NOEXIST)
    {
        // #ifdef DASICS_DEBUG
            my_printf("[ERROR]: Failed to find plt idx when do dasics_dynamic_call\n");
        // #endif
        exit(1);
    }
    // clear t3
    r_regs->t3 = 0;
    // catch malloc, free, realloc if you need
    if (!__dasics_linker_strcmp("malloc",_get_lib_name(entry, plt_idx)) && \
        !(entry->_flags & MAIN_AREA))
    {
    #ifdef DASICS_DEBUG
        my_printf("[DEBUG]: Try to call [malloc] on untrusted area [%s]!\n", entry->real_name);
    #endif            
        r_regs->a0 = entry->mem->malloc(entry->mem, r_regs->a0);
        r_regs->t1 = r_regs->ra;
        return NOEXEC;
    }

    if (!__dasics_linker_strcmp("free",_get_lib_name(entry, plt_idx)) && \
        !(entry->_flags & MAIN_AREA))
    {
    #ifdef DASICS_DEBUG
        my_printf("[DEBUG]: Try to call [free] on untrusted area [%s]!\n", entry->real_name);
    #endif            
        r_regs->a0 = entry->mem->free(entry->mem, r_regs->a0);
        r_regs->t1 = r_regs->ra;
        return NOEXEC;
    }

    if (!__dasics_linker_strcmp("realloc",_get_lib_name(entry, plt_idx)) && \
        !(entry->_flags & MAIN_AREA))
    {
    #ifdef DASICS_DEBUG
        my_printf("[DEBUG]: Try to call [realloc] on untrusted area [%s]!\n", entry->real_name);
    #endif            
        r_regs->a0 =  entry->mem->realloc(entry->mem, r_regs->a0, r_regs->a1);
        r_regs->t1 = r_regs->ra;
        return NOEXEC;
    }

    if (entry->_local_got[plt_idx + 2] == entry->plt_begin)
    {
        /*
        * We found that the Plt[x] wants to use dely binding to find the fucntion,
        * and we prepare all the parameters, and jump
        * 
        * dll_a0: the got[1], struct link_map of the library
        * dll_a1: the thrice of the plt table offset
        * ulib_func: the addr of the ulib function 
        */
        uint64_t dll_a0 = entry->map_link;
        uint64_t dll_a1 = (((reg_t)plt_idx * 0x10UL) >> 1) * 3;
        uint64_t ulib_func = entry->fixup_handler(dll_a0, dll_a1);
        ulib_func = _call_reloc(entry, ulib_func);
        target_got_entry = _get_trap_area(ulib_func);
        target = r_regs->t1 = ulib_func;

        /* saved */
        entry->_local_got[plt_idx + 2] = ulib_func;

        /* reset got */
        entry->got_begin[plt_idx + 2] = (uint64_t)dasics_umaincall;


    } else
    {
        /**
         * Now, the got has been filled with the lib function address in the memory
         * we will check it.
         */
        target = r_regs->t1 = entry->_local_got[plt_idx + 2];
        target_got_entry = _get_trap_area(target);
    }

    #ifdef DASICS_DEBUG
    my_printf("[DEBUG]: [%s] call dynamic [%s]'s lib func: %s, addr 0x%lx\n",entry->real_name, \
                                target_got_entry->real_name, _get_lib_name(entry, plt_idx), target);
    #endif

    // which means the dasicsReturnPC not equal dasicsReturnPC
    if (r_regs->dasicsReturnPC != r_regs->ra && \
                    (entry->_flags & MAIN_AREA) && \
                    !(target_got_entry->_flags & MAIN_AREA))
    {
    #ifdef DASICS_DEBUG
        my_printf("[Warning]: maybe used instruction j or jr to ulib func\n");
        my_printf("\tdasicsReturnPC: 0x%lx\n\tra: 0x%lx\n", r_regs->dasicsReturnPC,r_regs->ra );
    #endif
        r_regs->dasicsReturnPC = r_regs->ra;
    }


    if ((entry != target_got_entry) &&  // mean Cross call
        !(
            (entry->_flags & MAIN_AREA) &&
            (target_got_entry->_flags & MAIN_AREA)
            )                              // the entry and target both are trusted
        )
    {
    #ifdef DASICS_DEBUG
        my_printf("[DEBUG]: This is a Cross-library call\n");
    #endif
        push_libcfg(entry, target_got_entry, r_regs);
        r_regs->dasicsReturnPC = 0UL;
        r_regs->dasicsFreeZoneReturnPC = 0UL;
        r_regs->ra = (reg_t)dasics_umaincall;
        
        // TODO: clear pre libcfg and set new libcfg
        // TODO: better permission switching needed

        // TMP alloc a free zone for the lib func
        if (!(target_got_entry->_flags & MAIN_AREA))
        {   
            // the target's text will be set to free zone
            trap_libcfg_alloc(r_regs, \
                    DASICS_LIBCFG_X, \
                    target_got_entry->_text_end, \
                    target_got_entry->_plt_start);
            
            // the target's got table will set to be readed
            trap_libcfg_alloc(r_regs, \
                        DASICS_LIBCFG_R, \
                        (uint64_t)target_got_entry->got_begin + sizeof(uint64_t) * (target_got_entry->got_num  + 2), \
                        (uint64_t)target_got_entry->got_begin);    
        }        
    }
    
    return NORMAL_CALL;

}

/* This function is used to Cross-library call return */
int dasics_dynamic_return(regs_context_t * r_regs)
{
#ifdef DASICS_DEBUG
    my_printf("[DEBUG]: Cross-library call return!\n");
#endif
    cross_call_t * return_cross;
    // if ra equal (reg_t)dasics_umaincall, pop again
    // NOTE: we can call pop more than one time here
    // while (r_regs->ra == (reg_t)dasics_umaincall)
        return_cross = pop_libcfg(r_regs);
    r_regs->t1 = r_regs->ra;
#ifdef DASICS_DEBUG
    my_printf("[DEBUG]: Cross-library return from library [%s] to library: [%s]\n", \
        return_cross->target->real_name, return_cross->entry->real_name);
#endif

}