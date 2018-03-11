#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include "../include/kernel.h"

void syscall_handler(ExceptionInfo *ex_info) {
    TracePrintf(1, "this is syscall.");
    Halt();
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