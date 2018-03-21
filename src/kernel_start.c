#include <comp421/yalnix.h>
#include <stdlib.h>
#include <stdio.h>
#include <comp421/hardware.h>
#include <string.h>
#include "../include/kernel.h"

void *kernel_brk;
void *page_table_brk = (void *)VMEM_1_LIMIT;
int vm_enabled = FALSE;
unsigned int pid_count = 2; //starting with 2 because 1 & 0 are assigned to init and idle
unsigned int ff_count = 0; //free frame count
interruptHandlerType interrupt_vector[TRAP_VECTOR_SIZE];
struct free_frame *frame_list; //trace all the physical memory
struct free_page_table *page_table_list;
struct pcb *current_process;
struct pcb *idle_pcb;
struct pcb *init_pcb;
struct dequeue queues[NUM_QUEUES];
struct pcb *delay_list;
/////////////
struct pte *ptr0_idle;
struct pte *ptr0_init;


SavedContext *idle_ks_copy(SavedContext *ctxp, void *pcb_from, void *pcb_to);
//SavedContext *save_ctx(SavedContext *ctxp, void *pcb_from, void *pcb_to);
//SavedContext *do_nothing(SavedContext *ctxp, void *pcb_from, void *pcb_to) { return ctxp;};

void KernelStart(ExceptionInfo *info, unsigned int pmem_size, void *orig_brk, char **cmd_args) {
    kernel_brk = (void *) UP_TO_PAGE(orig_brk); //round up and save kernel_brk

    /*
     * initialize interrupt vector table
     */
    interrupt_vector[TRAP_KERNEL] = &syscall_handler;
    interrupt_vector[TRAP_CLOCK] = &clock_handler;
    interrupt_vector[TRAP_ILLEGAL] = &illegal_handler;
    interrupt_vector[TRAP_MEMORY] = &memory_handler;
    interrupt_vector[TRAP_MATH] = &math_handler;
    interrupt_vector[TRAP_TTY_RECEIVE] = &tty_receive_handler;
    interrupt_vector[TRAP_TTY_TRANSMIT] = &tty_transmit_handler;

    for (int i = TRAP_TTY_TRANSMIT + 1; i < TRAP_VECTOR_SIZE; i++) {
        interrupt_vector[i] = NULL;
    }
    WriteRegister(REG_VECTOR_BASE, (RCS421RegVal)interrupt_vector);

    //initialize a linked list of free frames to keep track of free page frames
    frame_list = (struct free_frame *) malloc(sizeof(struct free_frame));
    frame_list->pfn = 0; //pfn begins at 0
    ff_count++;
    struct free_frame *temp = frame_list;
    for (int i = 1; i < DOWN_TO_PAGE(pmem_size) >> PAGESHIFT; i++) {
        temp->next = (struct free_frame *) malloc(sizeof(struct free_frame));
        temp = temp->next;
        temp->pfn = i;
        ff_count++;
    }
    temp->next = NULL;

    /*
     *initialize free page table list for user processes
     */
    page_table_list = NULL;

    /*
     * initialization page table for region 0
     */
    ptr0_idle = malloc(PAGE_TABLE_SIZE);
    ptr0_init = malloc(PAGE_TABLE_SIZE);
    WriteRegister(REG_PTR0, (RCS421RegVal) ptr0_idle);
    //invalid segment
    for (int i = 0; i < MEM_INVALID_PAGES; i++) {
        ptr0_idle[i].valid = FALSE;
    }

    //kernel stack segment
    for (int i = KERNEL_STACK_BASE >> PAGESHIFT; i < KERNEL_STACK_LIMIT >> PAGESHIFT; i++) {
        ptr0_idle[i].pfn = i;
        ptr0_idle[i].uprot = PROT_NONE;
        ptr0_idle[i].kprot = PROT_READ | PROT_WRITE;
        ptr0_idle[i].valid = TRUE;
    }


    /*
     * initialization page table for region 1
     */
    struct pte *ptr1 = malloc(PAGE_TABLE_SIZE);
    WriteRegister(REG_PTR1, (RCS421RegVal) ptr1);

    //text segment
    int text_boundary = (unsigned long)(&_etext) >> PAGESHIFT;
    int j = 0;
    for (int i = VMEM_1_BASE >> PAGESHIFT; i < text_boundary; i++, j++) {
        ptr1[j].pfn = i;
        ptr1[j].uprot = PROT_NONE;
        ptr1[j].kprot = PROT_READ | PROT_EXEC;
        ptr1[j].valid = TRUE;
    }
    //heap segment
    int heap_boundary = (unsigned long) kernel_brk >> PAGESHIFT;
    for (int i = text_boundary; i < heap_boundary; i++, j++) {
        ptr1[j].pfn = i;
        ptr1[j].uprot = PROT_NONE;
        ptr1[j].kprot = PROT_READ | PROT_WRITE;
        ptr1[j].valid = TRUE;
    }


    /*
     * exclude the some allocated physical frames from the free frame list, because they are actually not free
     */
    temp = frame_list;
    while (temp->next != NULL) {
        struct free_frame *previous = temp;
        temp = temp->next;
        if (temp->pfn >= KERNEL_STACK_BASE >> PAGESHIFT && temp->pfn < (unsigned long)kernel_brk >> PAGESHIFT) {
            previous->next = temp->next;
            free(temp);
            temp = previous;
            ff_count--;
        }
    }

    /*
     * enable virtual memory
     */
    TracePrintf(0, "Enable the virtual memory.\n");
    WriteRegister(REG_VM_ENABLE, 1);
    vm_enabled = TRUE;

    /*
     * process scheduling queue initialization
     */
    for (int i = 0; i < NUM_QUEUES; i++) {
        queues[i].head = NULL;
        queues[i].tail = NULL;
    }
    delay_list = NULL;


    /*
     * create idle PCB, load idle process.
     */
    idle_pcb = malloc(sizeof(struct pcb));
    idle_pcb->pid = 0;
    idle_pcb->ptr0 = ptr0_idle;
    idle_pcb->ctx = malloc(sizeof(SavedContext));
    idle_pcb->countdown = 0;
    idle_pcb->child = NULL;
    idle_pcb->parent = NULL;
    idle_pcb->sibling = NULL;
    for (int i = 0; i < NUM_QUEUES + NUM_LISTS; i++) {
        idle_pcb->next[i] = NULL;
    }
    TracePrintf(0,"Kernelstart: Load idle process\n");

    //load program to idle process
    LoadProgram("./src/idle", cmd_args, info, idle_pcb); // initialize brk at the same time



    TracePrintf(0,"Kernelstart: idle process is successfully loaded.\n");

//    pcb_queue_add(READY_QUEUE, idle_pcb);//TODO just for test here, remove it later!

////    ContextSwitch(do_nothing, idle_pcb->ctx, idle_pcb, idle_pcb);//TODO test only
//    TracePrintf(0,"debug: back to do_nothing contextswtich\n");

    /*
     * create init PCB
     */
    init_pcb = malloc(sizeof(struct pcb));
//    struct free_page_table * ptttt= alloc_free_page_table();
//    ptr0_init = (struct pte *)(ptttt->vir_addr);//TODO test only delete later
//    test = (struct pte *)(ptttt->phy_addr);//TODO test only delete later
//    TracePrintf(0,"phy is %d, vir is %d.\n", test, ptr0_init);
    init_pcb->ptr0 = initialize_ptr0(ptr0_init);
    init_pcb->pid = 1;
    init_pcb->ctx = malloc(sizeof(SavedContext));
    init_pcb->countdown = 0;
    init_pcb->parent = NULL;
    init_pcb->child = NULL;
    init_pcb->sibling = NULL;
    for (int i = 0; i < NUM_QUEUES + NUM_LISTS; i++) {
        init_pcb->next[i] = NULL;
    }
    TracePrintf(0, "Kernelstart: Load init process\n");

//    current_process = init_pcb;//set current user process //already init pcb

    //copy 4 physical pages
//    ContextSwitch(idle_ks_copy, malloc(sizeof(SavedContext)), idle_pcb, init_pcb);//TODO test only


    ContextSwitch(idle_ks_copy, idle_pcb->ctx, idle_pcb, init_pcb); //TODO test!!!!!!
    if (current_process->pid == 0) {
        return; // return immediately
    }

    //load
    TracePrintf(0, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~!\n");
    LoadProgram("./src/init", cmd_args, info, init_pcb);
    //save context
//    ContextSwitch(save_ctx, idle_pcb->ctx, NULL, NULL); //TODO test!!!!!!
    TracePrintf(0, "!~~!~~~!~~~~!~~!~~~~~~!~~~~!~~~!~~~~~~~!~~~~~!~~~~!~~!\n");
    TracePrintf(0, "Kernelstart: kernel start complete!\n");
}

/*
 * allocate frames for kernel stack segment
 */
struct pte *initialize_ptr0(struct pte *ptr0) {
    //invalid segment
    for (int i = 0; i < MEM_INVALID_PAGES; i++) {
        ptr0[i].valid = FALSE;
    }

    //kernel stack segment
    for (int i = KERNEL_STACK_BASE >> PAGESHIFT; i < KERNEL_STACK_LIMIT >> PAGESHIFT; i++) {
        ptr0[i].pfn = get_free_frame();
        ptr0[i].uprot = PROT_NONE;
        ptr0[i].kprot = PROT_READ | PROT_WRITE;
        ptr0[i].valid = TRUE;
    }
    return ptr0;
}

/*
 * copy 4 physical pages
 */
SavedContext *idle_ks_copy(SavedContext *ctxp, void *pcb_from, void *pcb_to) {
    char kernel_stack_temp[KERNEL_STACK_PAGES * PAGESIZE];
    memcpy(kernel_stack_temp, (void *)KERNEL_STACK_BASE, KERNEL_STACK_PAGES * PAGESIZE); // save to temp
    // switch page table
    WriteRegister(REG_PTR0, (RCS421RegVal)((struct pcb *)pcb_to)->ptr0);
//    WriteRegister(REG_PTR0, (RCS421RegVal)test);//TODO test only change back later
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    memcpy((void *)KERNEL_STACK_BASE, kernel_stack_temp, KERNEL_STACK_PAGES * PAGESIZE); // copy 4 kernel stack pages
    TracePrintf(0, "ctx_swtich complete.\n");
    current_process = init_pcb;
    return ctxp;
}; //TODO test only

//SavedContext *save_ctx(SavedContext *ctxp, void *pcb_from, void *pcb_to) {
//    char kernel_stack_temp[KERNEL_STACK_PAGES * PAGESIZE];
//    memcpy(kernel_stack_temp, (void *)KERNEL_STACK_BASE, KERNEL_STACK_PAGES * PAGESIZE);
//    WriteRegister(REG_PTR0, (RCS421RegVal)idle_pcb->ptr0);
//    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
//    memcpy((void *)KERNEL_STACK_BASE, kernel_stack_temp, KERNEL_STACK_PAGES * PAGESIZE);
//
//    memcpy(init_pcb->ctx, idle_pcb->ctx, sizeof(SavedContext));
//    WriteRegister(REG_PTR0, (RCS421RegVal)init_pcb->ptr0);
//    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
//    return ctxp;
//}
