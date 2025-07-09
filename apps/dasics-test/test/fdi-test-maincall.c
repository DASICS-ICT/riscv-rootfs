#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "udasics.h"

#pragma GCC optimize("O0")
int ATTR_ULIB_TEXT test_maincall() {

	dasics_umaincall(Umaincall_PRINT, "[INFO] maincall print OK!\n");   // That's ok

	return 0;
}



int main() {
    printf("╔═══════════════════════════════════════╗\n");
    printf("║        FDI Test 4: maincall test      ║\n");
    printf("╚═══════════════════════════════════════╝\n");

	register_udasics(0);

	lib_call(&test_maincall);

	unregister_udasics();

	return 0;
}
