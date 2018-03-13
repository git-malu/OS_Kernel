/*
 * global variables
 */
extern int vm_enabled;
extern void *kernel_brk;
extern unsigned int ff_count;
extern struct free_frame *frame_list;

/*
 * constants
 */
enum boolean {FALSE, TRUE};

/*
 * System calls
 */
extern int my_Fork(void);
extern int my_Exec(char *filename, char **argvec, ExceptionInfo *ex_info);
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
typedef void (*interruptHandlerType) (ExceptionInfo *);
void syscall_handler(ExceptionInfo *);
void clock_handler(ExceptionInfo *);
void tty_receive_handler(ExceptionInfo *);
void tty_transmit_handler(ExceptionInfo *);
void illegal_handler(ExceptionInfo *);
void memory_handler(ExceptionInfo *);
void math_handler(ExceptionInfo *);

/*
 * memory utilities
 */
struct free_frame {
    int pfn;
    struct free_frame *next;
};
int verify_buffer(void *p, int len);
int verify_string(char *s);
int SetKernelBrk(void *addr);
void free_a_frame(unsigned int freed_pfn);
unsigned int get_free_frame();
int LoadProgram(char *name, char **args, ExceptionInfo *ex_info);
