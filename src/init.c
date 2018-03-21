#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include <stdio.h>


int main(int argc, char *argv[]) {
    printf("init process started! The pid is %d.\n", GetPid());
//    printf("Delay is called\n");
    Delay(5);
//    printf("return to init process from delay\n");
    int fork_return = Fork();
    printf("the return of Fork is %d.\n", fork_return);
    printf("after fork, the current pid is %d.\n", GetPid());
    Brk((void *)(50 * PAGESIZE));

//    while (1) {
//        Pause();
//        printf("Now in init process. Pause is released once. The pid is %d.\n", GetPid());
//        ;
//    }
    char *s[1];
    s[1] = NULL;
    Exec("./src/test1", s);
    return 0;
}
