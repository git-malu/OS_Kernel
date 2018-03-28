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
struct dequeue *queues[NUM_QUEUES];
struct pcb *lists[NUM_LISTS];
/////////////
struct pte *ptr0_idle;
struct pte *ptr0_init;
struct terminal terminals[NUM_TERMINALS];
char kernel_stack_temp[KERNEL_STACK_PAGES * PAGESIZE];


SavedContext *idle_ks_copy(SavedContext *ctxp, void *pcb_from, void *pcb_to);

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
     *initialize terminals
     */
    for (int i = 0; i < NUM_TERMINALS; i++) {
        terminals[i].current_writer = NULL;
        terminals[i].read_queue = malloc(sizeof(struct dequeue));
        terminals[i].read_queue->head = NULL;
        terminals[i].read_queue->tail = NULL;
        terminals[i].write_queue = malloc(sizeof(struct dequeue));
        terminals[i].write_queue->head = NULL;
        terminals[i].write_queue->tail = NULL;
        terminals[i].line_queue = malloc(sizeof(struct line_dequeue));
        terminals[i].line_queue->head = NULL;
        terminals[i].line_queue->tail = NULL;
    }

    /*
     * process scheduling queue initialization
     */

    for (int i = 0; i < NUM_QUEUES; i++) {
        queues[i] = malloc(sizeof(struct dequeue));
        (*queues[i]).head = NULL;
        (*queues[i]).tail = NULL;
    }
    lists[DELAY_LIST] = NULL;


    /*
     * create idle PCB, load idle process.
     */
    idle_pcb = malloc(sizeof(struct pcb));
    idle_pcb->pid = 0;
    idle_pcb->ptr0 = ptr0_idle;
    idle_pcb->ctx = malloc(sizeof(SavedContext));
    idle_pcb->countdown = 0;
    idle_pcb->child_list = NULL;
    idle_pcb->parent = NULL;
    idle_pcb->exit_queue = alloc_dequeue(); //allocate initialized dequeue
    idle_pcb->exit_status = 0;
    for (int i = 0; i < NUM_QUEUES + NUM_LISTS; i++) {
        idle_pcb->next[i] = NULL;
    }
    TracePrintf(0,"Kernelstart: Load idle process\n");

    //load program to idle process
    LoadProgram(cmd_args[0], cmd_args, info, idle_pcb); // initialize brk at the same time

    TracePrintf(0,"Kernelstart: idle process is successfully loaded.\n");


    /*
     * create init PCB
     */
    init_pcb = malloc(sizeof(struct pcb));
    init_pcb->ptr0 = initialize_ptr0(ptr0_init);
    init_pcb->pid = 1;
    init_pcb->ctx = malloc(sizeof(SavedContext));
    init_pcb->countdown = 0;
    init_pcb->parent = NULL;
    init_pcb->child_list = NULL;
    init_pcb->exit_queue = alloc_dequeue();//allocate initialized dequeue
    init_pcb->exit_status = 0;

    for (int i = 0; i < NUM_QUEUES + NUM_LISTS; i++) {
        init_pcb->next[i] = NULL;
    }
    TracePrintf(0, "Kernelstart: Load init process\n");

    ContextSwitch(idle_ks_copy, idle_pcb->ctx, idle_pcb, init_pcb); //
    if (current_process->pid == 0) {
        return; // return immediately
    }

    //load
    TracePrintf(0, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~!\n");
    LoadProgram("init", cmd_args, info, init_pcb);
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
    memcpy(kernel_stack_temp, (void *)KERNEL_STACK_BASE, KERNEL_STACK_PAGES * PAGESIZE); // save to temp
    // switch page table
    WriteRegister(REG_PTR0, (RCS421RegVal)((struct pcb *)pcb_to)->ptr0);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
    memcpy((void *)KERNEL_STACK_BASE, kernel_stack_temp, KERNEL_STACK_PAGES * PAGESIZE); // copy 4 kernel stack pages
    TracePrintf(0, "ctx_swtich complete.\n");
    current_process = init_pcb;
    return ctxp;
};


