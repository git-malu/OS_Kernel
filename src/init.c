#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include <stdio.h>


int main(int argc, char *argv[]) {
    printf("init process started! The pid is %d.\n", GetPid());
//    printf("Delay is called\n");
    Delay(5);
//    printf("return to init process from delay\n");
    int fork_return = Fork();
    if (fork_return == 0) {
        printf("the return of Fork is 0. now in child process, the pid is %d.\n", GetPid());
    } else {
        printf("the return of Fork is %d, now in parent process, parent pid is %d\n", fork_return, GetPid());
    }

//    Brk((void *)(50 * PAGESIZE));
//    char *s[1];
//    s[1] = NULL;
//    Exec("./src/test1", s);
    printf("End of program, exit. pid is %d.\n", GetPid());
    Exit(0);
}
