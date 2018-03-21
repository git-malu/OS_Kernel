#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include "../include/kernel.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    printf("init process started! The pid is %d.\n", GetPid());
//    printf("Delay is called\n");
    Delay(5);
//    printf("return to init process from delay\n");
    Fork();
    printf("after fork, the current pid is %d.", GetPid());
//    while (1) {
//        Pause();
//        printf("Now in init process. Pause is released once. The pid is %d.\n", GetPid());
//        ;
//    }
    return 0;
}
