#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include "../include/kernel.h"
int my_Fork(void) {
    return 0;
}

int my_Exec(char *filename, char **argvec, ExceptionInfo *ex_info) {
//    remember to call LoadProgram here
    if (verify_string(filename) < 0) {
        return ERROR;
    }
    int status = LoadProgram(filename, argvec, ex_info);
    switch (status) {
        case -1:
            return ERROR;
        case -2:
            my_Exit(-2);
        default:
            return 0;
    }
}

void my_Exit(int status) {
    Halt();
}

int my_Wait(int *status_ptr) {
    return 0;
}

int my_GetPid(void) {
    return 0;
}

int my_Brk(void *addr) {
    return 0;
}

int my_Delay(int clock_ticks) {
    return 0;
}

int my_TtyRead(int tty_id, void *buf, int len) {
    return 0;
}

int my_TtyWrite(int tty_id, void *buf, int len) {
    return 0;
}