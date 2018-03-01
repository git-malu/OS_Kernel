/*
 * global variables
 */
extern int vm_enabled;

/*
 * constants
 */
enum boolean {FALSE, TRUE};

/*
 * System calls
 */
extern int my_Fork(void);
extern int my_Exec(char *filename, char **argvec);
extern void my_Exit(int status);
extern int my_Wait(int *status_ptr);
extern int my_GetPid(void);
extern int my_Brk(void *addr);
extern int my_Delay(int clock_ticks);
extern int my_TtyRead(int tty_id, void *buf, int len);
extern int my_TtyWrite(int tty_id, void *buf, int len);

/*
 * interrupt, exception, trap handlers. (acronyms: ih, eh, th)
 */
void syscall_th(ExceptionInfo *);
void clock_ih(ExceptionInfo *);
void tty_receive_ih(ExceptionInfo *);
void tty_transmit_ih(ExceptionInfo *);
void illegal_eh(ExceptionInfo *);
void memory_eh(ExceptionInfo *);
void math_eh(ExceptionInfo *);

/*
 * memory utilities
 */
int verify_buffer(void *p, int len);
int verify_string(char *s);
int SetKernelBrk(void *addr);

