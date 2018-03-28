#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include "../include/kernel.h"
#include <string.h>
#include <stdlib.h>

//SavedContext *delay_to_idle(SavedContext *, void *, void *);
SavedContext *program_cpy(SavedContext *ctxp, void *from, void *to);


int kernel_Fork(void) {
    TracePrintf(0, "Syscall: kernel_Fork syscall is called.\n");
    struct free_page_table *new_pt_node = alloc_free_page_table(); //get a new page table who has continuous physical address
    struct pte *ptr0 = new_pt_node->vir_addr;
//    initialize_ptr0(ptr0);//allocate physical frames for kernel stack, and create corresponding pte
    struct pcb *new_pcb = create_child_pcb(ptr0, current_process);//create pcb for new process, initialize using parent info
    struct pcb *save_current_process = current_process;

    TracePrintf(0,"Lu Ma: Forking\n");
    ContextSwitch(program_cpy, current_process->ctx, current_process, new_pcb);//copy program to new process, copy pte protection to new process

    if (current_process->pid == save_current_process->pid) {
        TracePrintf(0,"   fork return as parent.\n");
        return new_pcb->pid;//Now I'm parent
    } else {
        TracePrintf(0,"   fork return as child.\n");
        return 0;//Now I'm child_list TODO check
    }
}

SavedContext *program_cpy(SavedContext *ctxp, void *from, void *to) {
    struct pcb *pcb_from = (struct pcb *)from;
    struct pcb *pcb_to = (struct pcb *)to;
    struct pte *src_ptr0 = pcb_from->ptr0;
    struct pte *dst_ptr0 = pcb_to->ptr0;

    //prepare address mapping for copy
    src_ptr0[USER_STACK_LIMIT >> PAGESHIFT].valid = 1;
    src_ptr0[USER_STACK_LIMIT >> PAGESHIFT].kprot = PROT_READ | PROT_WRITE;
    src_ptr0[USER_STACK_LIMIT >> PAGESHIFT].uprot = PROT_NONE;

    //start copy page by page
    for (int i = 0; i < PAGE_TABLE_LEN; i++) {
        if (i == USER_STACK_LIMIT >> PAGESHIFT) {
            continue; //skip it
        }
        dst_ptr0[i].valid = 0;
        if (src_ptr0[i].valid == 1) {
            dst_ptr0[i].valid = 1;
            dst_ptr0[i].kprot = src_ptr0[i].kprot;
            dst_ptr0[i].uprot = src_ptr0[i].uprot;

            dst_ptr0[i].pfn = get_free_frame();
            //copy one page from src to dst
            TracePrintf(10, "Lu Ma: assign a free frame(pfn) in program_cpy. the pfn is %d, the vpn is %d\n", dst_ptr0[i].pfn, i);
            WriteRegister(REG_TLB_FLUSH, USER_STACK_LIMIT); //flush just one page corresponding to USER_STACK_LIMIT
            src_ptr0[USER_STACK_LIMIT >> PAGESHIFT].pfn = dst_ptr0[i].pfn; //connect to same physical frame
            memcpy((void *)USER_STACK_LIMIT, (void *)(unsigned long)(i << PAGESHIFT), PAGESIZE);
        }
    }
    src_ptr0[USER_STACK_LIMIT >> PAGESHIFT].valid = 0; //recover mapping
//    TracePrintf(0, "Lu Ma: start context switch (WriteRegister) in program_cpy\n");
    RCS421RegVal phy_addr = vir2phy_addr((unsigned long)(((struct pcb *)to)->ptr0));
//    TracePrintf(0, "Lu Ma: the phy addr of dst_ptr in program_cpy is %d.\n", phy_addr);
    WriteRegister(REG_PTR0, phy_addr);//write the physical address of page table
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

    pcb_queue_add(READY_QUEUE, 0, pcb_from);// add 'from' to ready queue.
    current_process = (struct pcb *)to; //now in new_pcb process, that is, child process
    return ctxp;
}

int kernel_Exec(char *filename, char **argvec, ExceptionInfo *ex_info) {
    TracePrintf(0, "Syscall: kernel_Exec syscall is called.\n");
    int status = LoadProgram(filename, argvec, ex_info, current_process);
    switch (status) {
        case -1:
            return ERROR;
        case -2:
            kernel_Exit(-2); //unrecoverable
        default:
            return 0;
    }
}
/*
 * current process exits
 */
void kernel_Exit(int status) {
    TracePrintf(0, "Syscall: kernel_Exit syscall is called. \n");
    current_process->exit_status = status;
    // because this process as parent is exiting, set all children's parent field to null
    struct pcb *head = current_process->child_list;
    while (head != NULL) {
        head->parent = NULL;
        head = head->next[SIBLING_LIST];
    }

    //remove this process from its parent's child list
    if (current_process->parent != NULL) {
        struct pcb *previous = NULL;
        struct pcb *current = current_process->parent->child_list;

        while (current != NULL) {
            if (current == current_process) {
                //if found
                if (previous == NULL) {
                    current_process->parent->child_list = current->next[SIBLING_LIST];
                    current->next[SIBLING_LIST] = NULL;
                } else {
                    previous->next[SIBLING_LIST] = current->next[SIBLING_LIST];
                    current->next[SIBLING_LIST] = NULL;
                }
                break;

            } else {
                //if not found, advance
                previous = current;
                current = current->next[SIBLING_LIST];
            }
        }
    }

    // add to current process's parent exit queue
    pcb_queue_add(EXIT_QUEUE, 0, current_process);

    //context switch
    struct pcb *next_ready_pcb = pcb_queue_get(READY_QUEUE, 0, NULL);
    if (next_ready_pcb == NULL) {
        //go to idle
        TracePrintf(0, "the next_ready_pcb is NULL, so switch to idle.\n");
        ContextSwitch(to_idle, current_process->ctx, current_process, idle_pcb);
    } else {
        //go to next ready
        TracePrintf(0, "the next_ready_pcb is not NULL, so switch to next ready pcb. its pid is %d\n", next_ready_pcb->pid);
        ContextSwitch(to_next_ready, current_process->ctx, current_process, next_ready_pcb);
    }

}

SavedContext *to_idle(SavedContext *ctxp, void *from, void *to){
    RCS421RegVal phy_addr_ptr0 = vir2phy_addr((unsigned long)(((struct pcb *)to)->ptr0));
    WriteRegister(REG_PTR0, phy_addr_ptr0);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    current_process = (struct pcb *)to;
    return ((struct pcb *)to)->ctx;
}

SavedContext *to_next_ready(SavedContext *ctxp, void *from, void *to){
    RCS421RegVal phy_addr_ptr0 = vir2phy_addr((unsigned long)(((struct pcb *)to)->ptr0));
    WriteRegister(REG_PTR0, phy_addr_ptr0);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    current_process = (struct pcb *)to;
    return ((struct pcb *)to)->ctx;
}

/*
 * return the pid of the exit child
 * store child's status to *status_ptr
 */
int kernel_Wait(int *status_ptr) {
    TracePrintf(0, "Syscall: kernel_Wait syscall is called.\n");
    if (current_process->child_list == NULL) {
        //no children
        return ERROR;
    }

    struct pcb *exit_pcb = pcb_queue_get(EXIT_QUEUE, 0, current_process);
    if (exit_pcb == NULL) {
        pcb_list_add(WAIT_LIST, current_process); //add current process to wait list to wait for a child to exit
        struct pcb *next_ready_pcb = pcb_queue_get(READY_QUEUE, 0, NULL);
        if (next_ready_pcb == NULL) {
            //go to idle
            ContextSwitch(to_idle, current_process->ctx, current_process, idle_pcb);
        } else {
            //go the next ready
            ContextSwitch(to_next_ready, current_process->ctx, current_process, next_ready_pcb);
        }
        exit_pcb = pcb_queue_get(EXIT_QUEUE, 0, current_process);
    }

    //when come back from wait list
    *status_ptr = exit_pcb->exit_status;
    return exit_pcb->pid;
}


int kernel_GetPid(struct pcb *target_process) {
    unsigned int res = target_process->pid;
    TracePrintf(0, "Syscall: kernel_GetPid syscall is called. The pid is %d.\n", res);
    return res;
}

int kernel_Brk(void *addr, ExceptionInfo *ex_info) {
    TracePrintf(0, "Syscall: kernel_Brk syscall is called. \n");
    if (addr >= ex_info->sp) {
        return ERROR;
    }

    if (addr > current_process->brk) {
        //allocate memory
        TracePrintf(0, "allocate more memory for user process. set addr to %d.\n", addr);
        int orig_brk_index = ((unsigned long)(current_process->brk) >> PAGESHIFT);
        int new_brk_index = (UP_TO_PAGE(addr) >> PAGESHIFT);
        for (int i = orig_brk_index; i < new_brk_index; i++) {
            current_process->ptr0[i].valid = 1;
            current_process->ptr0[i].kprot = PROT_WRITE | PROT_READ;
            current_process->ptr0[i].uprot = PROT_WRITE | PROT_READ;
            current_process->ptr0[i].pfn = get_free_frame();
        }
    } else {
        //TODO release memory
    }
    return 0;
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
//    delay_list_add(current_process); // add to delay list
    pcb_list_add(DELAY_LIST, current_process); //add to delay list
    struct pcb * next_ready = pcb_queue_get(READY_QUEUE, 0, NULL);
    if (next_ready == NULL) {
        //go to idle
        TracePrintf(3, "calling delay, the next ready is null.\n");
        ContextSwitch(to_idle, current_process->ctx, current_process, idle_pcb);
    } else {
        //go to next ready
        TracePrintf(3, "calling delay, the next ready is pid %d.\n", next_ready->pid);
        ContextSwitch(to_next_ready, current_process->ctx, current_process, next_ready);
    }
    return 0;
}

//SavedContext *delay_to_idle(SavedContext *ctxp, void *pcb_from, void *pcb_to) {
//    WriteRegister(REG_PTR0, (RCS421RegVal)((struct pcb *)pcb_to)->ptr0);
//    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
//    current_process = pcb_to;
//    return ((struct pcb *)pcb_to)->ctx;
//};


/*
 * consumer
 */
int kernel_TtyRead(int tty_id, void *buf, int len) {
    int res;
    TracePrintf(0, "Syscall: kernel_TtyRead syscall is called.\n");
    if(tty_id >= NUM_TERMINALS || buf == NULL || len < 0 || len > TERMINAL_MAX_LINE) {
        return ERROR;
    }

//    struct pcb *next_read = pcb_queue_get(READ_QUEUE, tty_id, NULL);
    TracePrintf(0, "Syscall: kernel_TtyRead syscall is called. 2 \n");
    // see current reader
    if (terminals[tty_id].current_reader != NULL) {
        //queue up
        TracePrintf(0, "Syscall: kernel_TtyRead syscall is called. 3 \n");
        pcb_queue_add(READ_QUEUE, tty_id, current_process);
        idle_or_next_ready();
    }

    //no other process is reading
    terminals[tty_id].current_reader = current_process; //now I become the current reader


    //see if there is content to read
    struct line *target_line = line_queue_get(tty_id);
    if (target_line == NULL) {
        //nothing to read
        //leave but I'm still the current reader!
        TracePrintf(0, "   ttyread: target line is empty \n");
        idle_or_next_ready();
        target_line = line_queue_get(tty_id); //I'm back, update content!
    }

    TracePrintf(0, "Back to TtyRead!  kernel_TtyRead syscall is called. 4 \n");
    if (target_line->len <= len) {
        //read whole line
        memcpy(buf, target_line->s, target_line->len);
        res = target_line->len;
        TracePrintf(0, "Syscall: kernel_TtyRead syscall is called. 5 \n");
    } else {
        //read part of a line
        struct line *new_line = malloc(sizeof(struct line));
        new_line->s = malloc(target_line->len - len);
        new_line->len = target_line->len - len;
        TracePrintf(0, "Syscall: kernel_TtyRead syscall is called. 6 \n");
        if (terminals[tty_id].line_queue->head == NULL) {
            terminals[tty_id].line_queue->head = new_line;
            terminals[tty_id].line_queue->tail = new_line;
        } else {
            new_line->next = terminals[tty_id].line_queue->head;
            terminals[tty_id].line_queue->head = new_line;
        }
        TracePrintf(0, "Syscall: kernel_TtyRead syscall is called. 7 \n");
        memcpy(buf, target_line->s, len); //read
        memcpy(new_line->s, target_line->s + len, target_line->len - len); //cut
        res = len;
    }
    //read complete
    //remember to free memory
    free(target_line);
    terminals[tty_id].current_reader = NULL;
    //
    struct pcb *next_read = pcb_queue_get(READ_QUEUE, tty_id, NULL);
    if (next_read != NULL) {
        pcb_queue_add(READY_QUEUE, 0, current_process);
        ContextSwitch(ctx_sw, current_process->ctx, current_process, next_read);
    }
    //memory in kernel? yes, it's allocated by malloc in kernel mode.

    return res;
}

int kernel_TtyWrite(int tty_id, void *buf, int len) {
    if(tty_id >= NUM_TERMINALS || buf == NULL || len < 0 || len > TERMINAL_MAX_LINE) {
        return ERROR;
    }

    TracePrintf(0, "Syscall: kernel_TtyWrite syscall is called. the tty_id is %d\n", tty_id);

    if (terminals[tty_id].current_writer != NULL) {
        TracePrintf(0, "       writing is busy queue up\n");
        //terminal busy, add to WRITE_QUEUE
        pcb_queue_add(WRITE_QUEUE, tty_id, current_process);
        idle_or_next_ready(); //wait in I/O queue
        //after come back
    }
    //terminal not busy
    memcpy(terminals[tty_id].write_buffer, buf, len); //save the string to kernel memory
    terminals[tty_id].current_writer = current_process; // set busy
    TtyTransmit(tty_id, terminals[tty_id].write_buffer, len); // transmit a buffer of characters
    //transmitting, block the current process here
    return len;
}

void idle_or_next_ready() {
    struct pcb * next_ready = pcb_queue_get(READY_QUEUE, 0, NULL);
    if (next_ready == NULL) {
        //go to idle
        ContextSwitch(to_idle, current_process->ctx, current_process, idle_pcb);
    } else {
        //go to next ready
        ContextSwitch(to_next_ready, current_process->ctx, current_process, next_ready);
    }
}

