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



    TracePrintf(0, "clock_handler: delay_list_update complete\n");
    if (++robin_count == 2) {
        robin_count = 0;
        struct pcb *next_ready = pcb_queue_get(READY_QUEUE, NULL);
        if (next_ready == NULL) {
            TracePrintf(0, "Lu Ma: clock handler: next_ready is NULL\n");
            //if already in idle, remain in idle
            //if not in idle but in current process, remain in current process.
            //if exiting current process, context switch to idle (this should be done by kernel_exit)
        } else {
            //switch to next ready process
            if (current_process->pid != 0) {
                pcb_queue_add(READY_QUEUE, current_process); //add to the ready queue
            }
            TracePrintf(0, "Lu Ma: clock handler: switch to next ready.\n");
            ContextSwitch(to_next_ready, current_process->ctx, current_process, next_ready);
        }
    }
}



void tty_receive_handler(ExceptionInfo *ex_info) {

}

void tty_transmit_handler(ExceptionInfo *ex_info) {
    TracePrintf(0, "tty_transmit_handler is triggered.\n");
    int tty_id = ex_info->code;
    pcb_queue_add(READY_QUEUE, terminals[tty_id].process); //put into ready queue
}

void illegal_handler(ExceptionInfo *ex_info) {

}

void memory_handler(ExceptionInfo *ex_info) {

}

void math_handler(ExceptionInfo *ex_info) {

}




//SavedContext *sw_to_next_ready(SavedContext *ctxp, void *pcb_from, void *pcb_to) {
//    TracePrintf(9, "Context swtich: clock handler: now in my context switch function, the pcb_to is pid %d.\n", ((struct pcb *)pcb_to)->pid);
//    WriteRegister(REG_PTR0, (RCS421RegVal)((struct pcb *)pcb_to)->ptr0);
//    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
//    current_process = pcb_to;
//    if (((struct pcb *)pcb_from)->pid != 0) {
//        pcb_queue_add(READY_QUEUE, pcb_from); //add to the ready queue
//    }
//    TracePrintf(9, "Context swtich: clock handler: end of context switch funciton.\n"); //TODO test
//    return ((struct pcb *)pcb_to)->ctx;
//};

//SavedContext *sw_to_idle(SavedContext *ctxp, void *pcb_from, void *pcb_to) {
//    TracePrintf(0, "Context swtich: clock handler: switch from pid %d from idle process.\n", ((struct pcb *)pcb_from)->pid);
//    WriteRegister(REG_PTR0, (RCS421RegVal)((struct pcb *)pcb_to)->ptr0);
//    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
//    current_process = pcb_to;
//    return ((struct pcb *)pcb_to)->ctx;
//};

//SavedContext *sw_to_next_ready(SavedContext *ctxp, void *pcb_from, void *pcb_to) {
//    TracePrintf(0, "Context swtich: clock handler: now in my context switch function");
//    WriteRegister(REG_PTR0, (RCS421RegVal)((struct pcb *)pcb_to)->ptr0);
//    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
//    current_process = pcb_to;
//    pcb_queue_add(READY_QUEUE, pcb_from);//TODO unsure
//    TracePrintf(0, "ctx is %s when clock\n", *(idle_pcb->ctx)->s);//TODO test
//    return ((struct pcb *)pcb_to)->ctx;
//};
