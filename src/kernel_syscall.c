#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include "../include/kernel.h"
int kernel_Fork(void) {
    return 0;
}

int kernel_Exec(char *filename, char **argvec, ExceptionInfo *ex_info) {
//    remember to call LoadProgram here
    if (verify_string(filename) < 0) {
        return ERROR;
    }
    int status = LoadProgram(filename, argvec, ex_info, current_process);
    switch (status) {
        case -1:
            return ERROR;
        case -2:
            kernel_Exit(-2);
        default:
            return 0;
    }
}

void kernel_Exit(int status) {
    TracePrintf(0, "kernel_Exit syscall is called. System Halts.\n");
    Halt();
}

int kernel_Wait(int *status_ptr) {
    TracePrintf(0, "kernel_Wait syscall is called. System Halts.\n");
    Halt();
}

int kernel_GetPid(void) {
    TracePrintf(0, "kernel_GetPid syscall is called. System Halts.\n");
    Halt();
}

int kernel_Brk(void *addr) {
    TracePrintf(0, "kernel_Brk syscall is called. System Halts.\n");
    Halt();
}

int kernel_Delay(int clock_ticks) {
    TracePrintf(0, "kernel_Delay syscall is called. System Halts.\n");
    Halt();
}

int kernel_TtyRead(int tty_id, void *buf, int len) {
    TracePrintf(0, "kernel_TtyRead syscall is called. System Halts.\n");
    Halt();
}

int kernel_TtyWrite(int tty_id, void *buf, int len) {
    TracePrintf(0, "kernel_TtyWrite syscall is called. System Halts.\n");
    Halt();
}