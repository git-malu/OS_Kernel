/*
 * global variables
 */
extern int vm_enabled;
extern void *kernel_brk;
extern unsigned int ff_count;
extern struct free_frame *frame_list;
extern struct pcb *current_process;

/*
 * constants
 */
enum boolean {FALSE, TRUE};

/*
 * System calls
 */
extern int kernel_Fork(void);
extern int kernel_Exec(char *filename, char **argvec, ExceptionInfo *ex_info);
extern void kernel_Exit(int status);
extern int kernel_Wait(int *status_ptr);
extern int kernel_GetPid(void);
extern int kernel_Brk(void *addr);
extern int kernel_Delay(int clock_ticks);
extern int kernel_TtyRead(int tty_id, void *buf, int len);
extern int kernel_TtyWrite(int tty_id, void *buf, int len);

/*
 * interrupt, exception, trap handlers.
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
 * process utilities
 */
struct pcb {
    unsigned int pid;
    struct pte *ptr0;
    void *brk;

    //relations
    struct pcb *parent;
    struct pcb *child;
    struct pcb *sibling;
    struct pcb *next_ready; //ready queue
};

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
int LoadProgram(char *name, char **args, ExceptionInfo *ex_info, struct pcb *target_process);
void *create_new_ptr0();




struct dequeue {
    struct pcb *head;
    struct pcb *tail;
};