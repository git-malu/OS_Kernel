#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include "../include/kernel.h"
#include <stdlib.h>
#include <string.h>


int verify_buffer(void *p, int len) {
    return 0;
}

int verify_string(char *s) {
    struct pte* ptr0 = (struct pte*) ReadRegister(REG_PTR0);
    if (s == NULL) {
        return ERROR;
    }
    if (ptr0[DOWN_TO_PAGE(s) >> PAGESHIFT].valid != 1) {
        return ERROR;
    }
    if (ptr0[DOWN_TO_PAGE(s + strlen(s)) >> PAGESHIFT].valid != 1) {
        return ERROR;
    }
    //TODO: Access check has not been done
    return 0;
}

int SetKernelBrk(void *addr) { //I assume this addr is physical address both before and after vm_enabled
    addr = (void *)UP_TO_PAGE(addr); //round up
    if (vm_enabled) {
        //addr is virtual address
        TracePrintf(1, "Lu Ma: Setting kernel Brk, when VM is enabled.\n");
        // we need to update the mapping between vpn and pfn
        struct pte *ptr1 = (struct pte *)ReadRegister(REG_PTR1);
        if (addr - kernel_brk >= 0) {
            for (int i = (unsigned long)kernel_brk >> PAGESHIFT; i < (unsigned long)addr >> PAGESHIFT; i++) {
                int j = i - PAGE_TABLE_LEN;
                ptr1[j].pfn = get_free_frame();
                ptr1[j].uprot = PROT_NONE;
                ptr1[j].kprot = PROT_READ | PROT_WRITE;
                ptr1[j].valid = TRUE;
            }
            kernel_brk = addr;
        } else {
            TracePrintf(1, "Lu Ma: Error. set kernel brk backward.\n");
            //TODO free memory
        }


    } else {
        //addr is physical address
        TracePrintf(1, "Lu Ma: Setting kernel Brk when VM is not enabled yet.\n");
        kernel_brk = addr; //update kernel_brk
    }
    return 0;
}

unsigned int get_free_frame() {
    TracePrintf(10, "Lu Ma: get free frame.\n");
    if (ff_count <= 0) {
        return ERROR;
    }
    unsigned int res = frame_list->pfn;
    struct free_frame *old_head = frame_list;
    frame_list = frame_list->next;
    free(old_head);
    ff_count--;
    return res;
}

void free_a_frame(unsigned int freed_pfn) {
    struct free_frame *node = malloc(sizeof(struct free_frame));
    node->pfn = freed_pfn;
    node->next = frame_list;
    frame_list = node;
    ff_count++;
}

void pcb_queue_add(int q_name, struct pcb *target_pcb) {
    struct pcb *old_head = queues[q_name].head;
    struct pcb *old_tail = queues[q_name].tail;
    if (old_head == NULL) {
        queues[q_name].head = target_pcb;
        queues[q_name].tail = target_pcb;
    } else {
        old_tail->next[q_name] = target_pcb;
        queues[q_name].tail = target_pcb;
    }
}

struct pcb *pcb_queue_get(int q_name) {
    struct pcb *old_head = queues[q_name].head;
    struct pcb *old_tail = queues[q_name].tail;
    if (old_head == NULL) {
        return NULL;
    } else if (old_head == old_tail) {
        queues[q_name].head = NULL;
        queues[q_name].tail = NULL;
        return old_head;
    } else {
        queues[q_name].head = old_head->next[q_name];
        old_head->next[q_name] = NULL;
        return old_head;
    }
}

void delay_list_add(struct pcb *delayed_process) {
    struct pcb *old_head = delay_list;
    delayed_process->next[DELAY_LIST] = old_head;
    delay_list = delayed_process;
}


void delay_list_update() {
    struct pcb *previous = NULL;
    struct pcb *current = delay_list;
    //no item
    if (delay_list == NULL) {
        return;
    }

    while (current != NULL) {
        if (--(current->countdown)) {
            //not yet
            previous = current; //advance
            current = current->next[DELAY_LIST]; //advance
        } else {
            //time's up
            if (previous == NULL) {
                struct pcb *new_head = current->next[DELAY_LIST];
                current->next[DELAY_LIST] = NULL;
                pcb_queue_add(READY_QUEUE, current);
                delay_list = new_head;
                current = new_head;
            } else {
                struct pcb *new_head = current->next[DELAY_LIST];
                current->next[DELAY_LIST] = NULL;
                pcb_queue_add(READY_QUEUE, current);
                previous->next[DELAY_LIST] = new_head;
                current = new_head;
            }
        }
    }
}