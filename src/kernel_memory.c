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



/*
 * add to a pcb queue
 * target_pcb is the pcb you want to add to a queue
 */
void pcb_queue_add(int q_name, struct pcb *target_pcb) {
    if (q_name == EXIT_QUEUE) {
        //we need to find the corresponding parent of the exit queue
        queues[q_name] = target_pcb->parent->exit_queue;
    }
    struct pcb *old_head = queues[q_name].head;
    struct pcb *old_tail = queues[q_name].tail;
    if (old_head == NULL) {
        queues[q_name].head = target_pcb;
        queues[q_name].tail = target_pcb;
    } else {
        old_tail->next[q_name] = target_pcb;
        target_pcb->next[q_name] = NULL;
        queues[q_name].tail = target_pcb;
    }

}

/*
 * get from a pcb queue
 * target_pcb is whose queue you want to get a pcb from.
 * If you want to get from a global queue, set target_pcb to NULL and provide only global queue name.
 */
struct pcb *pcb_queue_get(int q_name, struct pcb *target_pcb) {
    struct pcb *res;
    if (target_pcb != NULL) {
        if (q_name == EXIT_QUEUE) {
            queues[q_name] = target_pcb->exit_queue;
        }
    }

    struct pcb *old_head = queues[q_name].head;
    struct pcb *old_tail = queues[q_name].tail;
    if (old_head == NULL) {
        res = NULL;
    } else if (old_head == old_tail) {
        queues[q_name].head = NULL;
        queues[q_name].tail = NULL;
        res = old_head;
    } else {
        queues[q_name].head = old_head->next[q_name];
        old_head->next[q_name] = NULL;
        res = old_head;
    }

    if (target_pcb != NULL) {
        if (q_name == EXIT_QUEUE) {
            target_pcb->exit_queue = queues[q_name];
        }
    }
    return res;
}

void delay_list_add(struct pcb *delayed_process) {
//    struct pcb *old_head = delay_list;
    TracePrintf(0, "debug: delay list add: the delay_list initial content was NULL? %d\n", delay_list);
    delayed_process->next[DELAY_LIST] = delay_list;
    delay_list = delayed_process;
}

/*
 * move process from delay list to ready queue when time is up.
 */
void delay_list_update() {
    TracePrintf(0,"     enter the delay_list_update\n");
    struct pcb *previous = NULL;
    struct pcb *current = delay_list;
    TracePrintf(0,"     enter the delay_list_update 2\n");
//    TracePrintf(0, "the current address is %d, the next address is %d!!!!!!!!!!!!!!\n", current, current->next[DELAY_LIST]);

    //no item
    if (delay_list == NULL) {
        TracePrintf(2, "Delay list update: there is no item in this list.\n");
        return;
    }
    TracePrintf(2, "Delay list update: enter loop !!!!\n");
    while (current != NULL) {

//        if (current->countdown == 0) {
//            TracePrintf(2, "Delay list update: in the loop, the countdown is 0 !!!!\n");
//            previous = current; //pointer advance
//            current = current->next[DELAY_LIST]; //pointer advance
//            continue;
//        }
//
        if (--(current->countdown)) {
            //not yet
            TracePrintf(2, "Delay list update: not yet, decreasing, current countdown is %d! !!!!\n", current->countdown);
            previous = current; //pointer advance
            current = current->next[DELAY_LIST]; //pointer advance
            continue;
        } else {
            //time's up, move this process out to ready queue
            TracePrintf(2, "Delay list update: time's up, countdown becomes %d!!!!!\n", current->countdown);
            if (previous == NULL) {
                TracePrintf(2, "Delay list update: previous is null!!!!!\n");
                struct pcb *new_head = current->next[DELAY_LIST];
                current->next[DELAY_LIST] = NULL;
                pcb_queue_add(READY_QUEUE, current); // add to ready queue
                delay_list = new_head;
                current = new_head; // current is updated
            } else {
                TracePrintf(2, "Delay list update: previous is not null!!!!!\n");
                struct pcb *new_head = current->next[DELAY_LIST];
                current->next[DELAY_LIST] = NULL;
                pcb_queue_add(READY_QUEUE, current); // add to ready queue
                previous->next[DELAY_LIST] = new_head;
                current = new_head; // current is updated
            }
        }
    }
    TracePrintf(2, "Delay list update: delay list udpate complete.\n");
}

/*
 * get a free page table who has continuous physical address
 */
struct free_page_table *alloc_free_page_table() {

    if (page_table_list == NULL) {
        //set page table brk
        if (page_table_brk - PAGESIZE >= kernel_brk) {
            page_table_brk -= PAGESIZE;
        }
        unsigned long pfn = get_free_frame();
        TracePrintf(0, "the pfn is %d.\n", pfn);
        struct free_page_table * new_pt1 = malloc(sizeof(struct free_page_table));
        struct free_page_table * new_pt2 = malloc(sizeof(struct free_page_table));

        new_pt1->pfn = new_pt2->pfn = pfn;
        new_pt1->next = new_pt2->next = NULL;
        new_pt1->free = new_pt2->free = TRUE;
        new_pt1->another_half = new_pt2; new_pt2->another_half = new_pt1;
        new_pt1->phy_addr = (void *)(pfn << PAGESHIFT); new_pt2->phy_addr = (void *) ((pfn << PAGESHIFT) + (unsigned long)PAGE_TABLE_SIZE);
        new_pt1->vir_addr = page_table_brk; new_pt2->vir_addr = page_table_brk + PAGE_TABLE_SIZE; //can be cast to struct pte *
        TracePrintf(0,"<< after shift phy is %d\n", (void *)(pfn << PAGESHIFT));

        page_table_list = new_pt1;
        new_pt1->next = new_pt2;

        struct pte *ptr1 = (struct pte *) ReadRegister(REG_PTR1);
        int index = ((unsigned long)page_table_brk - VMEM_REGION_SIZE) >> PAGESHIFT;
        ptr1[index].pfn = pfn;
        TracePrintf(0, "the pfn2 is %d, the index is %d.\n", pfn, index);
        ptr1[index].uprot = PROT_NONE;
        ptr1[index].kprot = PROT_READ | PROT_WRITE;
        ptr1[index].valid = TRUE;

        //chop
        page_table_list = new_pt2;
        new_pt1->next = NULL;
        new_pt1->free = FALSE;
        return new_pt1;
    } else {
        struct free_page_table *old_head = page_table_list;
        page_table_list = old_head->next;
        old_head->next = NULL;
        old_head->free = FALSE;
        return old_head;
    }

}

void free_a_page_table(struct free_page_table *to_be_freed) {
    to_be_freed->free = TRUE;
    to_be_freed->next = page_table_list;
    page_table_list = to_be_freed;
}


RCS421RegVal vir2phy_addr(unsigned long vaddr) {
    unsigned long index = (vaddr - VMEM_1_BASE) >> PAGESHIFT;
    struct pte *ptr1 = (struct pte *)ReadRegister(REG_PTR1);
    return (RCS421RegVal)(ptr1[index].pfn << PAGESHIFT | (vaddr & PAGEOFFSET));
}

/*
 * create new pcb for user process
 */
struct pcb *create_child_pcb(struct pte *ptr0, struct pcb *parent) {
    struct pcb *new_pcb = malloc(sizeof(struct pcb));
    new_pcb->ptr0 = ptr0;
    new_pcb->pid = pid_count++;
    new_pcb->ctx = malloc(sizeof(SavedContext));
    new_pcb->countdown = 0;
    new_pcb->brk = parent->brk;
    new_pcb->parent = parent; //set parent
    new_pcb->child_list = NULL;
    new_pcb->exit_status = 1;
    new_pcb->exit_queue = (struct dequeue) {NULL, NULL};

    pcb_list_add(CHILD_LIST, new_pcb); //record new pcb as a child of parent

    //initialize all the "next"s
    for (int i = 0; i < NUM_QUEUES + NUM_LISTS; i++) {
        new_pcb->next[i] = NULL;
    }

    return new_pcb;
}

/*
 * target_pcb is the pcb you want to add to a pcb list
 */
void pcb_list_add(int list_name, struct pcb* target_pcb) {
    if (list_name == CHILD_LIST) {
        target_pcb->next[SIBLING_LIST] = target_pcb->parent->child_list;
        target_pcb->parent->child_list = target_pcb;
    }
}

//void free_a_frame(unsigned int freed_pfn) {
//    struct free_frame *previous = NULL;
//    struct free_frame *current = frame_list;
//    struct free_frame *node = malloc(sizeof(struct free_frame));
//    node->pfn = freed_pfn;
//    if (current == NULL) {
//        frame_list = node;
//        ff_count++;
//        return;
//    }
//    //when current != NULL and previous == NULL
//    if (node->pfn < current->pfn) {
//        node->next = current;
//        frame_list = node;
//        ff_count++;
//        return;
//    } else {
//        previous = current;
//        current = current->next; //advance
//    }
//    //when there is only one node in the frame list
//    if (current == NULL) {
//        previous->next = node;
//        ff_count++;
//        return;
//    }
//    //when there are at least two nodes in the frame list. pointer (previous, current)
//    while (current != NULL) {
//        if (node->pfn < current->pfn) {
//            previous->next = node;
//            node->next = current;
//            ff_count++;
//            return;
//        } else {
//            previous = current;
//            current = current->next;//advance
//        }
//    }
//    previous->next = node;
//    ff_count++;
//}


//int get_continu_frames(int len) {
//    struct free_frame *current = frame_list;
//    struct free_frame *next = frame_list->next;
//    struct free_frame *break_node = NULL;
//    int i = 0;
//    int head_pfn;
//    if (current == NULL) {
//        return ERROR;
//    }
//    while (i != len) {
//        head_pfn = current->pfn;
//        for (i = 1; i < len; ++i) {
//            if (next == NULL) {
//                return ERROR; // we reach the end of list before we get enough continous free frame yet.
//            }
//            if (current->pfn + 1 == next->pfn) {
//                current = next;
//                next = next->next;
//                continue;
//            } else {
//                break_node = current;
//                current = next;
//                next = next->next;
//                break; // try again
//            }
//        }
//    }
//
//    if (break_node != NULL) {
//        break_node->next = next; // frame list head remains the same
//    } else {
//        frame_list = next; //new head
//    }
//
//    return head_pfn;
//}
