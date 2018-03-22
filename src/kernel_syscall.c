#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include "../include/kernel.h"
#include <string.h>

SavedContext *delay_to_idle(SavedContext *, void *, void *);
SavedContext *program_cpy(SavedContext *ctxp, void *from, void *to);
SavedContext *to_idle(SavedContext *ctxp, void *from, void *to);
SavedContext *to_next_ready(SavedContext *ctxp, void *from, void *to);

int kernel_Fork(void) {
    TracePrintf(0, "Syscall: kernel_Fork syscall is called.\n");
    struct free_page_table *new_pt_node = alloc_free_page_table(); //get a new page table who has continuous physical address
    struct pte *ptr0 = new_pt_node->vir_addr;
//    initialize_ptr0(ptr0);//allocate physical frames for kernel stack, and create corresponding pte
    struct pcb * new_pcb = create_child_pcb(ptr0, current_process);//create pcb for new process
    struct pcb *save_current_process = current_process;
    TracePrintf(0,"Lu Ma: start the context switch in the kernel_fork.\n");
    ContextSwitch(program_cpy, current_process->ctx, current_process, new_pcb);//copy program to new process, copy pte protection to new process
    TracePrintf(0,"fork context switch success\n");
    if (current_process->pid == save_current_process->pid) {
        return new_pcb->pid;//Now I'm parent
    } else {
        return 0;//Now I'm child_list TODO check
    }
}

SavedContext *program_cpy(SavedContext *ctxp, void *from, void *to) {
    struct pcb *pcb_from = (struct pcb *)from;
    struct pcb *pcb_to = (struct pcb *)to;
    struct pte *src_ptr0 = pcb_from->ptr0;
    struct pte *dst_ptr0 = pcb_to->ptr0;
//    //
//    pcb_to->parent = pcb_from;

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
            TracePrintf(0, "Lu Ma: assign a free frame(pfn) in program_cpy. the pfn is %d, the vpn is %d\n", dst_ptr0[i].pfn, i);
            WriteRegister(REG_TLB_FLUSH, USER_STACK_LIMIT); //flush just one page corresponding to USER_STACK_LIMIT
            src_ptr0[USER_STACK_LIMIT >> PAGESHIFT].pfn = dst_ptr0[i].pfn; //connect to same physical frame
            memcpy((void *)USER_STACK_LIMIT, (void *)(unsigned long)(i << PAGESHIFT), PAGESIZE);
        }
    }
    src_ptr0[USER_STACK_LIMIT >> PAGESHIFT].valid = 0; //recover mapping
    TracePrintf(0, "Lu Ma: start context switch (WriteRegister) in program_cpy\n");
    RCS421RegVal phy_addr = vir2phy_addr((unsigned long)(((struct pcb *)to)->ptr0));
    TracePrintf(0, "Lu Ma: the phy addr of dst_ptr in program_cpy is %d.\n", phy_addr);
    WriteRegister(REG_PTR0, phy_addr);//write the physical address of page table
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

    current_process = (struct pcb *)to;
    return ctxp;
}

int kernel_Exec(char *filename, char **argvec, ExceptionInfo *ex_info) {
    TracePrintf(0, "Syscall: kernel_Exec syscall is called.\n");
//    if (verify_string(filename) < 0) {
//        return ERROR;
//    }
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

void kernel_Exit(int status) {
    TracePrintf(0, "Syscall: kernel_Exit syscall is called. \n");
    current_process->exit_status = status;
    // set all children's parent to null
    struct pcb *head = current_process->child_list;
    while (head != NULL) {
        head->parent = NULL;
        head = head->next[SIBLING_LIST];
    }

    // add to current process's parent exit queue
    pcb_queue_add(EXIT_QUEUE, current_process);

    //context switch
    struct pcb *next_ready_pcb = pcb_queue_get(READY_QUEUE, NULL);
    if (next_ready_pcb == NULL) {
        //go to idle
        ContextSwitch(to_idle, current_process->ctx, current_process, idle_pcb);
    } else {
        //go to next ready
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


int kernel_Wait(int *status_ptr) {
    TracePrintf(0, "Syscall: kernel_Wait syscall is called. System Halts.\n");
    Halt();
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
