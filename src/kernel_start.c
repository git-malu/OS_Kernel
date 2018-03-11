#include <comp421/yalnix.h>
#include <stdlib.h>
#include <comp421/hardware.h>
#include "../include/kernel.h"

void *kernel_brk;
int vm_enabled = FALSE;
unsigned int ff_count = 0; //free frame count
interruptHandlerType interrupt_vector[TRAP_VECTOR_SIZE];
struct free_frame *frame_list; //trace all the physical memory


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
    frame_list -> pfn = 0; //pfn begins at 0
    ff_count++;
    struct free_frame *temp = frame_list;
    for (int i = 1; i < DOWN_TO_PAGE(pmem_size) >> PAGESHIFT; i++) {
        temp -> next = (struct free_frame *) malloc(sizeof(struct free_frame));
        temp = temp -> next;
        temp -> pfn = i;
        ff_count++;
    }
    temp -> next = NULL;


    /*
     * initialization page table for region 1
     */
    struct pte *ptr1 = malloc(PAGE_TABLE_SIZE);
    WriteRegister(REG_PTR1, (RCS421RegVal) ptr1);

    //text segment
    int text_boundary = (unsigned long)(&_etext) >> PAGESHIFT;
    for (int i = VMEM_1_BASE >> PAGESHIFT; i < text_boundary; i++) {
        ptr1[i].pfn = i;
        ptr1[i].uprot = PROT_NONE;
        ptr1[i].kprot = PROT_READ | PROT_EXEC;
        ptr1[i].valid = TRUE;
    }
    //heap segment
    int heap_boundary = (unsigned long) kernel_brk >> PAGESHIFT;
    for (int i = text_boundary; i < heap_boundary; i++) {
        ptr1[i].pfn = i;
        ptr1[i].uprot = PROT_NONE;
        ptr1[i].kprot = PROT_READ | PROT_WRITE;
        ptr1[i].valid = TRUE;
    }

    /*
     * initialization page table for region 0
     */
    struct pte *ptr0 = malloc(PAGE_TABLE_SIZE);
    WriteRegister(REG_PTR0, (RCS421RegVal) ptr0);
    for (int i = KERNEL_STACK_BASE >> PAGESHIFT; i < KERNEL_STACK_LIMIT >> PAGESHIFT; i++) {
        ptr0[i].pfn = i;
        ptr0[i].uprot = PROT_NONE;
        ptr0[i].kprot = PROT_READ | PROT_WRITE;
        ptr0[i].valid = TRUE;
    }


    /*
     * exclude the allocated physical frames from the free frame list
     */
    temp = frame_list;
    while (temp -> next != NULL) {
        struct free_frame *previous = temp;
        temp = temp -> next;
        if (temp -> pfn >= KERNEL_STACK_BASE >> PAGESHIFT && temp -> pfn < (unsigned long)kernel_brk >> PAGESHIFT) {
            previous -> next = temp -> next;
            free(temp);
            temp = previous;
            ff_count--;
        }
    }

    /*
     * enable virtual memory
     */
    WriteRegister(REG_VM_ENABLE, 1);
    vm_enabled = TRUE;
}


