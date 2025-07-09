#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>
#include "udasics.h"

static char bounded_data[100][64];
static int func_id;

#define FUNCTIONS \
    F(0)   F(1)   F(2)   F(3)   F(4)   F(5)   F(6)   F(7)   F(8)   F(9)   \
    F(10)  F(11)  F(12)  F(13)  F(14)  F(15)  F(16)  F(17)  F(18)  F(19)  \
    F(20)  F(21)  F(22)  F(23)  F(24)  F(25)  F(26)  F(27)  F(28)  F(29)  \
    F(30)  F(31)  F(32)  F(33)  F(34)  F(35)  F(36)  F(37)  F(38)  F(39)  \
    F(40)  F(41)  F(42)  F(43)  F(44)  F(45)  F(46)  F(47)  F(48)  F(49)  \
    F(50)  F(51)  F(52)  F(53)  F(54)  F(55)  F(56)  F(57)  F(58)  F(59)  \
    F(60)  F(61)  F(62)  F(63)  F(64)  F(65)  F(66)  F(67)  F(68)  F(69)  \
    F(70)  F(71)  F(72)  F(73)  F(74)  F(75)  F(76)  F(77)  F(78)  F(79)  \
    F(80)  F(81)  F(82)  F(83)  F(84)  F(85)  F(86)  F(87)  F(88)  F(89)  \
    F(90)  F(91)  F(92)  F(93)  F(94)  F(95)  F(96)  F(97)  F(98)  F(99)

#define F(n) void __attribute__((section(".ulibtext"),aligned(8))) func_##n() { \
    dasics_umaincall(Umaincall_PRINT, "[INFO] function %d try to store to the bounded data..\n", func_id); \
    bounded_data[func_id][32] = 'B'; \
}

FUNCTIONS  

#undef F 

#pragma GCC pop_options
void exit_function() {
	printf("[ERROR] FDI Test 6 FAILED..\n");
}

int main(int argc, char *argv[]) {
    atexit(exit_function);
    register_udasics(0);

    printf("╔═══════════════════════════════════════╗\n");
    printf("║  FDI Test 6: flexible protection test ║\n");
    printf("╚═══════════════════════════════════════╝\n");
    
    if (argc < 2) {
        printf("Error: Missing function ID or command\n\n");
        return 1;
    }
    
    func_id = atoi(argv[1]);
    if (func_id < 0 || func_id > 99) {
        printf("Error: Function ID must be between 0 and 99\n");
        return 1;
    }

    int32_t idx_0 = dasics_libcfg_alloc(DASICS_LIBCFG_R  , (uint64_t)&func_id - 8, (uint64_t)&func_id + 8);

    void (*funcs[100])(void) = {
        #define F(n) func_##n,
        FUNCTIONS
        #undef F
    };
    
    int32_t idx_1 = dasics_libcfg_alloc(DASICS_LIBCFG_R  , (uint64_t)bounded_data[func_id], (uint64_t)64);
    printf("[INFO] call function %d \n",func_id);
    lib_call(funcs[func_id]);

    dasics_libcfg_free(idx_0);
    dasics_libcfg_free(idx_1);
    
	unregister_udasics();

    return 0;
}