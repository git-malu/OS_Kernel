#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include "../include/kernel.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    printf("now in idle process.\n");
    //infinite loop
    while (1) {
        Pause();
    }
    return 0;
}