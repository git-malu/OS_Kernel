#include <comp421/yalnix.h>
#include <comp421/hardware.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define	TTY_WRITE_STR(term, str) TtyWrite(term, str, strlen(str))
char line[TERMINAL_MAX_LINE];

#define MAX_ARGC	32

int
StartTerminal(int i)
{
    char *cmd_argv[MAX_ARGC];
    char numbuf[128];	/* big enough for %d */
    int pid;

    if (i == TTY_CONSOLE)
        cmd_argv[0] = "console";
    else
        cmd_argv[0] = "shell";
    sprintf(numbuf, "%d", i);
    cmd_argv[1] = numbuf;
    cmd_argv[2] = NULL;

    TracePrintf(0, "Pid %d calling Fork\n", GetPid());
    pid = Fork();
    TracePrintf(0, "Pid %d got %d from Fork\n", GetPid(), pid);

    if (pid < 0) {
        TtyPrintf(TTY_CONSOLE,
                  "Cannot Fork control program for terminal %d.\n", i);
        return (ERROR);
    }

    if (pid == 0) {
        Exec(cmd_argv[0], cmd_argv);
        TtyPrintf(TTY_CONSOLE,
                  "Cannot Exec control program for terminal %d.\n", i);
        Exit(1);
    }

    TtyPrintf(TTY_CONSOLE, "Started pid %d on terminal %d\n", pid, i);
    return (pid);
}

int
main(int argc, char **argv)
{
    int pids[NUM_TERMINALS];
    int i;
    int status;
    int pid;
    printf("hello world\n");
    for (i = 0; i < NUM_TERMINALS; i++) {
        pids[i] = StartTerminal(i);
        if ((i == TTY_CONSOLE) && (pids[TTY_CONSOLE] < 0)) {
            TtyPrintf(TTY_CONSOLE, "Cannot start Console monitor!\n");
            Exit(1);
        }
    }

    while (1) {
        pid = Wait(&status);
        if (pid == pids[TTY_CONSOLE]) {
            TtyPrintf(TTY_CONSOLE, "Halting Yalnix\n");
            /*
             *  Halt should normally be a privileged instruction (and
             *  thus not usable from user mode), but the hardware
             *  has been set up to allow it for this project so that
             *  we can shut down Yalnix simply here.
             */
            Halt();
        }
        for (i = 1; i < NUM_TERMINALS; i++) {
            if (pid == pids[i]) break; //if the pid is one of the valid pids, break;
        }
        if (i < NUM_TERMINALS) {
            TtyPrintf(TTY_CONSOLE, "Pid %d exited on terminal %d.\n", pid, i);
            pids[i] = StartTerminal(i);
        }
        else {
            TtyPrintf(TTY_CONSOLE, "Mystery pid %d returned from Wait!\n", pid);
        }
    }
}


//
//char *words = "hello world\n";
//int main(int argc, char *argv[]) {
//    int len;
//
//    fprintf(stderr, "Doing TtyRead...\n");
//    len = TtyRead(0, line, TERMINAL_MAX_LINE);
//    fprintf(stderr, "Done with TtyRead: len %d\n", len);
//    fprintf(stderr, "line '");
//    fwrite(line, len, 1, stderr);
//    fprintf(stderr, "'\n");
//
//    Exit(0);
////    int *status_ptr = malloc(sizeof(int));
////    printf("init process started! The pid is %d.\n", GetPid());
////    TtyPrintf(TTY_CONSOLE, words);
////    Delay(5);
////    TtyWrite(TTY_CONSOLE, words, strlen(words));
//
////    int fork_return = Fork();
////    if (fork_return == 0) {
////        printf("the return of Fork is 0. now in child process, the pid is %d.\n", GetPid());
////        Delay(5);
////    } else {
////        printf("the return of Fork is %d, now in parent process, parent pid is %d\n", fork_return, GetPid());
////        printf("    start waiting\n");
////        Wait(status_ptr);
////        printf("wait finished, now in parent process. pid is %d\n", GetPid());
////    }
////
//////    Brk((void *)(50 * PAGESIZE));
//////    char *s[1];
//////    s[1] = NULL;
//////    Exec("./src/test1", s);
////    if (fork_return == 0) {
////        printf("child program finihsed! exiting\n");
////    } else {
////        printf("parent program finihsed! exiting\n");
////    }
//    Exit(0);
//}
