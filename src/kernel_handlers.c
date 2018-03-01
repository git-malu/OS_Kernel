#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include "../include/kernel.h"

void syscall_th(ExceptionInfo *ex_info) {
    TracePrintf(1, "this is syscall.");
    Halt();
}

void clock_ih(ExceptionInfo *ex_info) {

}

void tty_receive_ih(ExceptionInfo *ex_info) {

}

void tty_transmit_ih(ExceptionInfo *ex_info) {

}

void illegal_eh(ExceptionInfo *ex_info) {

}

void memory_eh(ExceptionInfo *ex_info) {

}

void math_eh(ExceptionInfo *ex_info) {

}