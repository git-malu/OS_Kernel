#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include "../include/kernel.h"

SavedContext *delay_to_idle(SavedContext *, void *, void *);

int kernel_Fork(void) {
    TracePrintf(0, "Syscall: kernel_Fork syscall is called.\n");
    return 0;
}

int kernel_Exec(char *filename, char **argvec, ExceptionInfo *ex_info) {
    TracePrintf(0, "Syscall: kernel_Exec syscall is called.\n");
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
    TracePrintf(0, "Syscall: kernel_Exit syscall is called. System Halts.\n");
    Halt();
}

int kernel_Wait(int *status_ptr) {
    TracePrintf(0, "Syscall: kernel_Wait syscall is called. System Halts.\n");
    Halt();
}

int kernel_GetPid(struct pcb *target_process) {
    unsigned int res = target_process->pid;
    TracePrintf(0, "Syscall: kernel_GetPid syscall is called. The pid is %d.\n", res);
    return res;
}

int kernel_Brk(void *addr) {
    TracePrintf(0, "Syscall: kernel_Brk syscall is called. System Halts.\n");
    Halt();
}

/*
 * delay
 */
int kernel_Delay(int clock_ticks) {
    TracePrintf(0, "Syscall: kernel_Delay syscall is called. Context switch to idle process.\n");
    if (clock_ticks <= 0) {
        return 0;//return immediately
    }
    current_process->countdown = clock_ticks;
    delay_list_add(current_process); // add to delay list
    ContextSwitch(delay_to_idle, current_process->ctx, current_process, idle_pcb);
    return 0;
}

SavedContext *delay_to_idle(SavedContext *ctxp, void *pcb_from, void *pcb_to) {
    WriteRegister(REG_PTR0, (RCS421RegVal)((struct pcb *)pcb_to)->ptr0);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    current_process = pcb_to;
    return ((struct pcb *)pcb_to)->ctx;
};


/*
 * terminal
 */
int kernel_TtyRead(int tty_id, void *buf, int len) {
    TracePrintf(0, "Syscall: kernel_TtyRead syscall is called. System Halts.\n");
    Halt();
}

int kernel_TtyWrite(int tty_id, void *buf, int len) {
    TracePrintf(0, "Syscall: kernel_TtyWrite syscall is called. System Halts.\n");
    Halt();
}

