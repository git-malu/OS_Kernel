#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include "../include/kernel.h"
//    #define	YALNIX_FORK		1
//    #define	YALNIX_EXEC		2
//    #define	YALNIX_EXIT		3
//    #define	YALNIX_WAIT		4
//    #define   YALNIX_GETPID   5
//    #define	YALNIX_BRK		6
//    #define	YALNIX_DELAY    7
//
//    #define	YALNIX_TTY_READ		21
//    #define	YALNIX_TTY_WRITE	22
void syscall_handler(ExceptionInfo *ex_info) {

    TracePrintf(1, "Now enter syscall_handler. The code is %d.\n", ex_info->code);

    switch (ex_info->code) {
        case YALNIX_FORK:
            break;
        case YALNIX_EXEC:
            break;
        case YALNIX_EXIT:
            kernel_Exit(0); //TODO should status be 0?
            break;
        case YALNIX_WAIT:
            break;
        case YALNIX_GETPID:
            break;
        case YALNIX_BRK:
            break;
        case YALNIX_DELAY:
            break;
        case YALNIX_TTY_READ:
            break;
        case YALNIX_TTY_WRITE:
            break;
        default:
            break;
    }

//    Halt();
}

void clock_handler(ExceptionInfo *ex_info) {

}

void tty_receive_handler(ExceptionInfo *ex_info) {

}

void tty_transmit_handler(ExceptionInfo *ex_info) {

}

void illegal_handler(ExceptionInfo *ex_info) {

}

void memory_handler(ExceptionInfo *ex_info) {

}

void math_handler(ExceptionInfo *ex_info) {

}