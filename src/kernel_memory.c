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
    if (ReadRegister(REG_VM_ENABLE)) {
        //addr is virtual address
        TracePrintf(1, "Setting kernel Brk, VM is enabled.");
        kernel_brk = addr;
        // we need to update the mapping between vpn and pfn
        struct pte *ptr1 = (struct pte *)ReadRegister(REG_PTR1);
        if (addr - kernel_brk >= 0) {
            TracePrintf(1, "Lu Ma: Setting kernel Brk when VM is enabled.\n");
            for (int i = (unsigned long)kernel_brk >> PAGESHIFT; i < (unsigned long)addr >> PAGESHIFT; i++) {
                int j = i - PAGE_TABLE_LEN;
                ptr1[j].pfn = get_free_frame();
                ptr1[j].uprot = PROT_NONE;
                ptr1[j].kprot = PROT_READ | PROT_WRITE;
                ptr1[j].valid = TRUE;
            }
        } else {
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
    if (ff_count <= 0) {
        return ERROR;
    }
    unsigned int res = frame_list->pfn;
    struct free_frame *old_head = frame_list;
    frame_list = frame_list->next;
    free(old_head);
    return res;
}

void free_a_frame(unsigned int freed_pfn) {
    struct free_frame *node = malloc(sizeof(struct free_frame));
    node->pfn = freed_pfn;
    node->next = frame_list;
    frame_list = node;
}
