#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    printf("this is test1 program! the pid is %d.\n", GetPid());
    printf("test1 program finished. exit\n");
    return 0;
}
