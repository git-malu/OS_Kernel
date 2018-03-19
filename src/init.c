#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include "../include/kernel.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    printf("init process started! The pid is %d.\n", GetPid());
    while (1) {
        Pause();
        printf("Now in init process. Pause is released once. The pid is %d.\n", GetPid());
        ;
    }
    return 0;
}