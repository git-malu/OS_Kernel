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
 * tty_id is for you to choose queue from different terminals, if you don't need it, just set it to 0.
 */
void pcb_queue_add(int q_name, int tty_id, struct pcb *target_pcb) {
    //set non-global queue
    if (q_name == EXIT_QUEUE) {
        //we will find the corresponding parent of the exit queue
        if (target_pcb->parent == NULL) {
            return; //if no parent, just discard.
        }
        queues[q_name] = target_pcb->parent->exit_queue; //set exit queue
    }

    //global queue
    if (q_name == WRITE_QUEUE) {
        queues[q_name] = terminals[tty_id].write_queue;
    }

    struct pcb *old_head = queues[q_name]->head;
    struct pcb *old_tail = queues[q_name]->tail;
    if (old_head == NULL) {
        queues[q_name]->head = target_pcb;
        queues[q_name]->tail = target_pcb;
    } else {
        old_tail->next[q_name] = target_pcb;
        target_pcb->next[q_name] = NULL;
        queues[q_name]->tail = target_pcb;
    }

}

/*
 * get from a pcb queue
 * target_pcb is whose queue you want to get a pcb from. such as parent's exit queue
 * If you want to get from a global queue, set target_pcb to NULL and provide only global queue name.
 */
struct pcb *pcb_queue_get(int q_name, int tty_id, struct pcb *target_pcb) {
    struct pcb *res;
    //set non-global queue
    if (target_pcb != NULL) {
        if (q_name == EXIT_QUEUE) {
            queues[q_name] = target_pcb->exit_queue;
        }
    }

    //global queue
    if (q_name == WRITE_QUEUE) {
        queues[q_name] = terminals[tty_id].write_queue;
    }

    struct pcb *old_head = queues[q_name]->head;
    struct pcb *old_tail = queues[q_name]->tail;
    if (old_head == NULL) {
        res = NULL;
    } else if (old_head == old_tail) {
        queues[q_name]->head = NULL;
        queues[q_name]->tail = NULL;
        res = old_head;
    } else {
        queues[q_name]->head = old_head->next[q_name];
        old_head->next[q_name] = NULL;
        res = old_head;
    }

    return res;
}



/*
 * move process from delay list to ready queue when time is up.
 */
void delay_list_update() {
    TracePrintf(0,"     enter the delay_list_update\n");
    struct pcb *previous = NULL;
    struct pcb *current = lists[DELAY_LIST];
    TracePrintf(0,"     enter the delay_list_update 2\n");

    //no item
    if (lists[DELAY_LIST] == NULL) {
        TracePrintf(2, "Delay list update: there is no item in this list.\n");
        return;
    }
    TracePrintf(2, "Delay list update: enter loop !!!!\n");
    while (current != NULL) {

        if (--(current->countdown)) {
            //not yet
            TracePrintf(2, "Delay list update: not yet, decreasing, current countdown is %d! !!!!\n", current->countdown);
            previous = current; //pointer advance
            current = current->next[DELAY_LIST]; //pointer advance
            continue;
        } else {
            //time's up, move this process from delay list to ready queue
            TracePrintf(2, "Delay list update: time's up, countdown becomes %d!!!!!\n", current->countdown);
            if (previous == NULL) {
                TracePrintf(2, "Delay list update: previous is null!!!!!\n");
                struct pcb *new_head = current->next[DELAY_LIST];
                current->next[DELAY_LIST] = NULL;
                pcb_queue_add(READY_QUEUE, 0, current); // add to ready queue
                lists[DELAY_LIST] = new_head;
                current = new_head; // current is updated
            } else {
                TracePrintf(2, "Delay list update: previous is not null!!!!!\n");
                struct pcb *new_head = current->next[DELAY_LIST];
                current->next[DELAY_LIST] = NULL;
                pcb_queue_add(READY_QUEUE, 0, current); // add to ready queue
                previous->next[DELAY_LIST] = new_head;
                current = new_head; // current is updated
            }
        }
    }
    TracePrintf(2, "Delay list update: delay list udpate complete.\n");
}
/*
 * check all the waiting processes, and move whose exit_queue's head is no longer null to the ready queue.
 * return TRUE, if found
 * reuturn FALSE, if not found
 */
void wait_list_update() {
    struct pcb *previous = NULL;
    struct pcb *current = lists[WAIT_LIST];
    while (current != NULL) {
        if (current->exit_queue->head == NULL) {
            //if not found, advance
            previous = current;
            current = current->next[WAIT_LIST];
        } else {
            //if found, remove and advance
            if (previous == NULL) {
                //add to ready queue
                pcb_queue_add(READY_QUEUE, 0, current);
                //remove and advance
                lists[WAIT_LIST] = current->next[WAIT_LIST];
                current->next[WAIT_LIST] = NULL;
                current = lists[WAIT_LIST];//advance
            } else {
                //add to ready queue
                pcb_queue_add(READY_QUEUE, 0, current);
                //remove and advance
                previous->next[WAIT_LIST] = current->next[WAIT_LIST];
                current->next[WAIT_LIST] = NULL;
                current = previous->next[WAIT_LIST];
            }
        }
    }
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

/*
 * it's meant only for page table address translation
 */
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
    new_pcb->exit_status = 0;
    new_pcb->exit_queue = alloc_dequeue();//allocate initialized dequeue

    pcb_list_add(CHILD_LIST, new_pcb); //record new pcb as a child of parent

    //initialize all the "next"s
    for (int i = 0; i < NUM_QUEUES + NUM_LISTS; i++) {
        new_pcb->next[i] = NULL;
    }

    return new_pcb;
}

//void delay_list_add(struct pcb *delayed_process) {
////    struct pcb *old_head = delay_list;
//    TracePrintf(0, "debug: delay list add: the delay_list initial content was NULL? %d\n", lists[DELAY_LIST]);
//    delayed_process->next[DELAY_LIST] = lists[DELAY_LIST];
//    lists[DELAY_LIST] = delayed_process;
//}

/*
 * target_pcb is the pcb you want to add to a pcb list
 */
void pcb_list_add(int list_name, struct pcb* target_pcb) {
    //non-global list
    if (list_name == CHILD_LIST) {
        //add the target_pcb as a child
        target_pcb->next[SIBLING_LIST] = target_pcb->parent->child_list;
        target_pcb->parent->child_list = target_pcb;
        return;
    }

    //global list
    target_pcb->next[list_name] = lists[list_name];
    lists[list_name] = target_pcb;
}

struct dequeue *alloc_dequeue() {
    struct dequeue *res = malloc(sizeof(struct dequeue));
    res->head = NULL;
    res->tail = NULL;
    return res;
}

void line_queue_add(int tty_id, struct line *target_line) {
    struct line *old_head = terminals[tty_id].line_queue->head;
    struct line *old_tail = terminals[tty_id].line_queue->tail;

//    struct pcb *old_head = queues[q_name]->head;
//    struct pcb *old_tail = queues[q_name]->tail;
    if (old_head == NULL) {
        terminals[tty_id].line_queue->head = target_line;
        terminals[tty_id].line_queue->tail = target_line;
//        queues[q_name]->head = target_pcb;
//        queues[q_name]->tail = target_pcb;
    } else {
        old_tail->next = target_line;
        target_line->next = NULL;
        terminals[tty_id].line_queue->tail = target_line; //update pointer
//        old_tail->next[q_name] = target_pcb;
//        target_pcb->next[q_name] = NULL;
//        queues[q_name]->tail = target_pcb;
    }
}

struct line *line_queue_get(int tty_id) {
    struct line *res;
    struct line *old_head = terminals[tty_id].line_queue->head;
    struct line *old_tail = terminals[tty_id].line_queue->tail;

//    struct pcb *old_head = queues[q_name]->head;
//    struct pcb *old_tail = queues[q_name]->tail;
    if (old_head == NULL) {
        res = NULL;
    } else if (old_head == old_tail) {
        terminals[tty_id].line_queue->head = NULL;
        terminals[tty_id].line_queue->tail = NULL;
        res = old_head;
    } else {
        terminals[tty_id].line_queue->head = old_head->next;
        old_head->next = NULL;
        res = old_head;
    }

    return res;
}

