#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include "../include/kernel.h"
#include <string.h>

SavedContext *delay_to_idle(SavedContext *, void *, void *);
SavedContext *program_cpy(SavedContext *ctxp, void *from, void *to);

int kernel_Fork(void) {
    TracePrintf(0, "Syscall: kernel_Fork syscall is called.\n");
    struct free_page_table *new_pt_node = alloc_free_page_table(); //get a new page table who has continuous physical address
    struct pte *ptr0 = new_pt_node->vir_addr;
//    initialize_ptr0(ptr0);//allocate physical frames for kernel stack, and create corresponding pte
    struct pcb * new_pcb = create_pcb(ptr0);//create pcb for new process
    struct pcb *save_current_process = current_process;
    TracePrintf(0,"Lu Ma: start the context switch in the kernel_fork.\n");
    ContextSwitch(program_cpy, current_process->ctx, current_process, new_pcb);//copy program to new process, copy pte protection to new process
    TracePrintf(0,"fork context switch success\n");
    if (current_process->pid == save_current_process->pid) {
        return new_pcb->pid;//Now I'm parent
    } else {
        return 0;//Now I'm child TODO check
    }
}

SavedContext *program_cpy(SavedContext *ctxp, void *from, void *to) {
//    char buffer[VMEM_REGION_SIZE - MEM_INVALID_SIZE];
    struct pcb *pcb_from = (struct pcb *)from;
    struct pcb *pcb_to = (struct pcb *)to;
    struct pte *src_ptr0 = pcb_from->ptr0;
    struct pte *dst_ptr0 = pcb_to->ptr0;
//    struct pte *ptr1 = (struct pte *)ReadRegister(REG_PTR1);
    TracePrintf(0, "Lu Ma: initialize in program_cpy\n");
    //prepare address mapping for copy
    src_ptr0[USER_STACK_LIMIT >> PAGESHIFT].valid = 1;
    src_ptr0[USER_STACK_LIMIT >> PAGESHIFT].kprot = PROT_READ | PROT_WRITE;
    src_ptr0[USER_STACK_LIMIT >> PAGESHIFT].uprot = PROT_NONE;
    TracePrintf(0, "Lu Ma: start looping in program_cpy\n");
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
//    memcpy(buffer, (void *)MEM_INVALID_SIZE, VMEM_REGION_SIZE - MEM_INVALID_SIZE); //copy the whole region to buffer
    RCS421RegVal phy_addr = vir2phy_addr((unsigned long)(((struct pcb *)to)->ptr0));
    TracePrintf(0, "Lu Ma: the phy addr of dst_ptr in program_cpy is %d.\n", phy_addr);
    WriteRegister(REG_PTR0, phy_addr);//write the physical address of page table
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
//    memcpy((void *)MEM_INVALID_SIZE, buffer, VMEM_REGION_SIZE - MEM_INVALID_SIZE);//copy the whole region from buffer to new process
//    struct pte *ptr0_from = ((struct pcb *)from)->ptr0;
//    struct pte *ptr0_to = ((struct pcb *)to)->ptr0;
//
//    //copy pte
//    for (int i = VMEM_0_BASE; i < VMEM_REGION_SIZE; i++) {
//        ptr0_to[i].valid = ptr0_from[i].valid;
//        ptr0_to[i].kprot = ptr0_from[i].kprot;
//        ptr0_to[i].uprot = ptr0_from[i].uprot;
//    }

    //copy context!
//    memcpy(((struct pcb *)to)->ctx, ((struct pcb *)from)->ctx, sizeof(SavedContext));//TODO

    current_process = (struct pcb *)to;
    return ctxp;
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
