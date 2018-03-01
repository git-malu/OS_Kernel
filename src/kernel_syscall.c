#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include "../include/kernel.h"
int my_Fork(void) {

}

int my_Exec(char *filename, char **argvec) {
//    remember to call LoadProgram here
    verify_string(filename);

}

void my_Exit(int status) {

}

int my_Wait(int *status_ptr) {

}

int my_GetPid(void) {

}

int my_Brk(void *addr) {

}

int my_Delay(int clock_ticks) {

}

int my_TtyRead(int tty_id, void *buf, int len) {

}

int my_TtyWrite(int tty_id, void *buf, int len) {

}