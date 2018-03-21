#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include <stdlib.h>
#include "../include/kernel.h"
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
SavedContext *sw_to_next_ready(SavedContext *ctxp, void *pcb_from, void *pcb_to);
SavedContext *sw_to_idle(SavedContext *ctxp, void *pcb_from, void *pcb_to);

void syscall_handler(ExceptionInfo *ex_info) {

    TracePrintf(1, "Now enter syscall_handler. The code is %d.\n", ex_info->code);

    switch (ex_info->code) {
        case YALNIX_FORK:
            ex_info->regs[0] = kernel_Fork();
            break;
        case YALNIX_EXEC:
            kernel_Exec((char *)(ex_info->regs[1]), (char **)(ex_info->regs[2]), ex_info);
            break;
        case YALNIX_EXIT:
            kernel_Exit(0); //TODO should status be 0?
            break;
        case YALNIX_WAIT:
            break;
        case YALNIX_GETPID:
            ex_info->regs[0] = kernel_GetPid(current_process);
            break;
        case YALNIX_BRK:
            kernel_Brk((void *) (ex_info->regs[1]), ex_info);
            break;
        case YALNIX_DELAY:
            kernel_Delay(ex_info->regs[1]);
            break;
        case YALNIX_TTY_READ:
            break;
        case YALNIX_TTY_WRITE:
            break;
        default:
            break;
    }
}

void clock_handler(ExceptionInfo *ex_info) {
    static int robin_count = 0;
    TracePrintf(2, "clock_handler is triggered. the tick count is %d.\n", robin_count);

    delay_list_update(); //check all the delayed processes, and move whose time's up to the ready queue
    TracePrintf(0, "clock_handler: delay_list_update complete\n");
    if (++robin_count == 2) { //TODO >=2
        robin_count = 0;
        struct pcb *ready_next = pcb_queue_get(READY_QUEUE);
        if (ready_next == NULL) {
            TracePrintf(0, "Lu Ma: clock handler: ready_next is NULL\n");
//            if (current_process->pid == 0) {
//                return;
//            } else {
//                //switch to idle
//                TracePrintf(0, "Lu Ma: clock handler: switch to idle.\n");
//                ContextSwitch(sw_to_idle, current_process->ctx, current_process, idle_pcb);
//            }
        } else {
            //switch to next ready process
            TracePrintf(0, "Lu Ma: clock handler: switch to next ready.\n");
            ContextSwitch(sw_to_next_ready, current_process->ctx, current_process, ready_next);
        }
    }
//    //TODO add delay syscall processing
}

SavedContext *sw_to_next_ready(SavedContext *ctxp, void *pcb_from, void *pcb_to) {
    TracePrintf(0, "Context swtich: clock handler: now in my context switch function, the pcb_to is pid %d.\n", ((struct pcb *)pcb_to)->pid);
    WriteRegister(REG_PTR0, (RCS421RegVal)((struct pcb *)pcb_to)->ptr0);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    current_process = pcb_to;
    pcb_queue_add(READY_QUEUE, pcb_from); //add to the ready queue
    TracePrintf(0, "Context swtich: clock handler: end of context switch funciton.\n"); //TODO test
    return ((struct pcb *)pcb_to)->ctx;
};

SavedContext *sw_to_idle(SavedContext *ctxp, void *pcb_from, void *pcb_to) {
    TracePrintf(0, "Context swtich: clock handler: switch from pid %d from idle process.\n", ((struct pcb *)pcb_from)->pid);
    WriteRegister(REG_PTR0, (RCS421RegVal)((struct pcb *)pcb_to)->ptr0);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    current_process = pcb_to;
    return ((struct pcb *)pcb_to)->ctx;
};

//SavedContext *sw_to_next_ready(SavedContext *ctxp, void *pcb_from, void *pcb_to) {
//    TracePrintf(0, "Context swtich: clock handler: now in my context switch function");
//    WriteRegister(REG_PTR0, (RCS421RegVal)((struct pcb *)pcb_to)->ptr0);
//    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
//    current_process = pcb_to;
//    pcb_queue_add(READY_QUEUE, pcb_from);//TODO unsure
//    TracePrintf(0, "ctx is %s when clock\n", *(idle_pcb->ctx)->s);//TODO test
//    return ((struct pcb *)pcb_to)->ctx;
//};


void tty_receive_handler(ExceptionInfo *ex_info) {

}

void tty_transmit_handler(ExceptionInfo *ex_info) {

}

void illegal_handler(ExceptionInfo *ex_info) {

}

void memory_handler(ExceptionInfo *ex_info) {

}

void math_handler(ExceptionInfo *ex_info) {

}
