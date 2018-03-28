#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include <stdlib.h>
#include "../include/kernel.h"
#include <string.h>
#include <stdio.h>
//    #define	YALNIX_FORK		1
//    #define	YALNIX_EXEC		2
//    #define	YALNIX_EXIT		3
//    #define	YALNIX_WAIT		4
//    #define   YALNIX_GETPID   5
//    #define	YALNIX_BRK		6
//    #define	YALNIX_DELAY    7
//
//    #define	YALNIX_TTY_READ		21
//    #define	YALNIX_TTY_WRITE	22

SavedContext *to_write(SavedContext *ctxp, void *from, void *to);


void syscall_handler(ExceptionInfo *ex_info) {

    TracePrintf(1, "Now enter syscall_handler. The code is %d.\n", ex_info->code);

    switch (ex_info->code) {
        case YALNIX_FORK:
            ex_info->regs[0] = kernel_Fork();
            break;
        case YALNIX_EXEC:
            ex_info->regs[0] = kernel_Exec((char *)(ex_info->regs[1]), (char **)(ex_info->regs[2]), ex_info);
            break;
        case YALNIX_EXIT:
            kernel_Exit(ex_info->regs[1]);
            break;
        case YALNIX_WAIT:
            ex_info->regs[0] = kernel_Wait((int *)ex_info->regs[1]);
            break;
        case YALNIX_GETPID:
            ex_info->regs[0] = kernel_GetPid(current_process);
            break;
        case YALNIX_BRK:
            ex_info->regs[0] = kernel_Brk((void *) (ex_info->regs[1]), ex_info);
            break;
        case YALNIX_DELAY:
            ex_info->regs[0] = kernel_Delay(ex_info->regs[1]);
            break;
        case YALNIX_TTY_READ:
            ex_info->regs[0] = kernel_TtyRead((int)(ex_info->regs[1]), (void *)(ex_info->regs[2]), (int)(ex_info->regs[3]));
            break;
        case YALNIX_TTY_WRITE:
            ex_info->regs[0] = kernel_TtyWrite((int)(ex_info->regs[1]), (void *)(ex_info->regs[2]), (int)(ex_info->regs[3]));
            break;
        default:
            break;
    }
}

void clock_handler(ExceptionInfo *ex_info) {
    static int robin_count = 0;
    TracePrintf(2, "clock_handler is triggered. the tick count is %d.\n", robin_count);
    //update
    delay_list_update(); //check all the delayed processes, and move whose time's up to the ready queue
    wait_list_update(); //check all the waiting processes, and move whose exit_queue's head is no longer null to the ready queue.

    if (++robin_count == 2) {
        robin_count = 0;
        struct pcb *next_ready = pcb_queue_get(READY_QUEUE, 0, NULL);
        if (next_ready == NULL) {
            TracePrintf(0, "Lu Ma: clock handler: next_ready is NULL\n");
            //if already in idle, remain in idle
            //if not in idle but in current process, remain in current process.
            //if exiting current process, context switch to idle (this should be done by kernel_exit)
        } else {
            //switch to next ready process
            if (current_process->pid != 0) {
                pcb_queue_add(READY_QUEUE, 0, current_process); //add to the ready queue
            }
            TracePrintf(0, "Lu Ma: clock handler: switch to next ready.\n");
            ContextSwitch(to_next_ready, current_process->ctx, current_process, next_ready);
        }
    }
}


/*
 * producer
 */
void tty_receive_handler(ExceptionInfo *ex_info) {
    int tty_id = ex_info->code;
    int rec_len = TtyReceive(tty_id, terminals[tty_id].read_buffer, TERMINAL_MAX_LINE);
    struct line *new_line = malloc(sizeof(struct line));
    new_line->next = NULL;
    new_line->len = rec_len;
    new_line->s = malloc(sizeof(rec_len));
    memcpy(new_line->s, terminals[tty_id].read_buffer, rec_len); //fill in the content
    line_queue_add(tty_id, new_line);

    //anyone waiting?
    if (terminals[tty_id].current_reader != NULL) {
        pcb_queue_add(READY_QUEUE, 0, current_process); //save current process to ready queue
        ContextSwitch(ctx_sw, current_process->ctx, current_process, terminals[tty_id].current_reader);
    }
}

SavedContext *ctx_sw(SavedContext *ctxp, void *from, void *to){
    RCS421RegVal phy_addr_ptr0 = vir2phy_addr((unsigned long)(((struct pcb *)to)->ptr0));
    WriteRegister(REG_PTR0, phy_addr_ptr0);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    current_process = (struct pcb *)to;
    return ((struct pcb *)to)->ctx;
}

void tty_transmit_handler(ExceptionInfo *ex_info) {
    TracePrintf(0, "tty_transmit_handler is triggered.\n");
    int tty_id = ex_info->code;
    terminals[tty_id].current_writer = NULL; // clear, not busy anymore
    //get the write_queue continue moving
    struct pcb *next_write = pcb_queue_get(WRITE_QUEUE, tty_id, NULL);
    if (next_write != NULL) {
        pcb_queue_add(READY_QUEUE, 0, current_process); //save current process to ready queue
        ContextSwitch(to_write, current_process->ctx, current_process, next_write);
    }
}

SavedContext *to_write(SavedContext *ctxp, void *from, void *to){
    RCS421RegVal phy_addr_ptr0 = vir2phy_addr((unsigned long)(((struct pcb *)to)->ptr0));
    WriteRegister(REG_PTR0, phy_addr_ptr0);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    current_process = (struct pcb *)to;
    return ((struct pcb *)to)->ctx;
}

void illegal_handler(ExceptionInfo *ex_info) {
    int pid = current_process->pid;
    switch(ex_info->code){
        case TRAP_ILLEGAL_ILLOPC:
            printf("Illegal opcode. The pid of current process is %d.\n", pid);
            break;
        case TRAP_ILLEGAL_ILLOPN:
            printf("Illegal operand. The pid of current process is %d.\n", pid);
            break;
        case TRAP_ILLEGAL_ILLADR:
            printf("Illegal addressing mode. The pid of current process is %d.\n", pid);
            break;
        case TRAP_ILLEGAL_ILLTRP:
            printf("Illegal software trap. The pid of current process is %d.\n", pid);
            break;
        case TRAP_ILLEGAL_PRVOPC:
            printf("Privileged opcode. The pid of current process is %d.\n", pid);
            break;
        case TRAP_ILLEGAL_PRVREG:
            printf("Privileged register. The pid of current process is %d.\n", pid);
            break;
        case TRAP_ILLEGAL_COPROC:
            printf("Coprocessor error. The pid of current process is %d.\n", pid);
            break;
        case TRAP_ILLEGAL_BADSTK:
            printf("Bad stack. The pid of current process is %d.\n", pid);
            break;
        case TRAP_ILLEGAL_KERNELI:
            printf("Linux kernel sent SIGILL. The pid of current process is %d.\n", pid);
            break;
        case TRAP_ILLEGAL_USERIB:
            printf("Received SIGILL or SIGBUS from user. The pid of current process is %d.\n", pid);
            break;
        case TRAP_ILLEGAL_ADRALN:
            printf("Invalid address alignment. The pid of current process is %d.\n", pid);
            break;
        case TRAP_ILLEGAL_ADRERR:
            printf("Non-existent physical address. The pid of current process is %d.\n", pid);
            break;
        case TRAP_ILLEGAL_OBJERR:
            printf("Object-specific HW error. The pid of current process is %d.\n", pid);
            break;
        case TRAP_ILLEGAL_KERNELB:
            printf("Linux kernel sent SIGBUS. The pid of current process is %d.\n", pid);
            break;
        default:
            break;
    }
    kernel_Exit(ERROR);
}

void memory_handler(ExceptionInfo *ex_info) {
    unsigned long addr = ex_info->addr;
    if (addr >= (current_process->brk + PAGESIZE) && addr < USER_STACK_LIMIT) {
        current_process->ptr0[addr >> PAGESHIFT].valid = 1;
        current_process->ptr0[addr >> PAGESHIFT].uprot = PROT_READ | PROT_WRITE;
        current_process->ptr0[addr >> PAGESHIFT].kprot = PROT_READ | PROT_WRITE;
        current_process->ptr0[addr >> PAGESHIFT].pfn = get_free_frame();
    } else {
        kernel_Exit(ERROR);
    }
}

void math_handler(ExceptionInfo *ex_info) {
    int pid = current_process->pid;
    switch (ex_info->code) {
        case TRAP_MATH_INTDIV:
            printf("Integer divide by zero. The pid of current process is %d.\n", pid);
            break;
        case TRAP_MATH_INTOVF:
            printf("Integer overflow. The pid of current process is %d.\n", pid);
            break;
        case TRAP_MATH_FLTDIV:
            printf("Floating divide by zero. The pid of current process is %d.\n", pid);
            break;
        case TRAP_MATH_FLTOVF:
            printf("Floating overflow. The pid of current process is %d.\n", pid);
            break;
        case TRAP_MATH_FLTUND:
            printf("Floating underflow. The pid of current process is %d.\n", pid);
            break;
        case TRAP_MATH_FLTRES:
            printf("Floating inexact result. The pid of current process is %d.\n", pid);
            break;
        case TRAP_MATH_FLTINV:
            printf("Invalid floating operation. The pid of current process is %d.\n", pid);
            break;
        case TRAP_MATH_FLTSUB:
            printf("FP subscript out of range. The pid of current process is %d.\n", pid);
            break;
        case TRAP_MATH_KERNEL:
            printf("Linux kernel sent SIGFPE. The pid of current process is %d.\n", pid);
            break;
        case TRAP_MATH_USER:
            printf("Received SIGFPE from user. The pid of current process is %d.\n", pid);
            break;
        default:
            break;
    }
    kernel_Exit(ERROR);
}


