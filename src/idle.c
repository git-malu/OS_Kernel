#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    printf("Now in idle process. The pid is %d.\n", GetPid());
    //infinite loop
    while (1) {
        Pause();
        printf("Now in idle process. Pause is released once. The pid is %d.\n", GetPid());
    }
}
