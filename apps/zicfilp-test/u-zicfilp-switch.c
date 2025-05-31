#include <stdio.h>
#include <sys/prctl.h>
#include <unistd.h>

int main(){
    printf("Zicfilp test starts!\n");
    int i = 0;
    // while(i==0);

    /* Enable zicfilp function in U-mode */
    int error = prctl(1000, 2, 1, 0, 0);

    // printf("Zicfilp test Finishes\n");
    // while (1)
    // {
    // }
    return 0;
}