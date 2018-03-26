#define NUM_QUEUES 4
#define NUM_LISTS 4


/*
 * global variables
 */
extern int vm_enabled;
extern void *kernel_brk;
extern void *page_table_brk;
extern unsigned int pid_count;
extern unsigned int ff_count;
extern struct free_frame *frame_list;
extern struct free_page_table *page_table_list;
extern struct pcb *current_process;
extern struct pcb *idle_pcb;
extern struct pcb *init_pcb;
extern struct pcb *delay_list;
extern struct dequeue {
    struct pcb *head;
    struct pcb *tail;
} *queues[];
extern struct pcb *lists[];
extern struct terminal {
    struct pcb *process;
    char read_buffer[TERMINAL_MAX_LINE];
    char write_buffer[TERMINAL_MAX_LINE];
    struct dequeue *write_queue;
    struct dequeue *read_queue;
} terminals[];

/*
 * constants
 */
enum boolean {FALSE, TRUE};
enum list_type {READY_QUEUE, EXIT_QUEUE, READ_QUEUE, WRITE_QUEUE, DELAY_LIST, WAIT_LIST, CHILD_LIST, SIBLING_LIST}; //queues come first, then list follows

/*
 * System calls
 */
extern int kernel_Fork(void);
extern int kernel_Exec(char *filename, char **argvec, ExceptionInfo *ex_info);
extern void kernel_Exit(int status);
extern int kernel_Wait(int *status_ptr);
extern int kernel_GetPid(struct pcb *target_process);
extern int kernel_Brk(void *addr, ExceptionInfo *ex_info);
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
    SavedContext *ctx;
    int countdown; //clock
    int exit_status;

    //relations
    struct pcb *parent;
    struct pcb *child_list; //every pcb has a child_list
    struct dequeue *exit_queue; //every pcb has a exit queue for collecting children's exit status
    struct pcb *next[NUM_QUEUES + NUM_LISTS];//
};

SavedContext *to_idle(SavedContext *ctxp, void *from, void *to);
SavedContext *to_next_ready(SavedContext *ctxp, void *from, void *to);



/*
 * memory utilities
 */
struct free_frame {
    int pfn;
    struct free_frame *next;
};

struct free_page_table {
    int free;
    unsigned int pfn;
    void *vir_addr;
    void *phy_addr;
    struct free_page_table *another_half;
    struct free_page_table *next;
};

int verify_buffer(void *p, int len);
int verify_string(char *s);
int SetKernelBrk(void *addr);
void free_a_frame(unsigned int freed_pfn);
unsigned int get_free_frame();
int LoadProgram(char *name, char **args, ExceptionInfo *ex_info, struct pcb *target_process);
struct pte *initialize_ptr0(struct pte *ptr0);
void pcb_queue_add(int q_name, int tty_id, struct pcb *target_pcb);
struct pcb *pcb_queue_get(int q_name, int tty_id, struct pcb *target_pcb);
//void delay_list_add(struct pcb *delayed_process);
void delay_list_update();
struct free_page_table *alloc_free_page_table();
struct pcb *create_child_pcb(struct pte *ptr0, struct pcb *parent);
RCS421RegVal vir2phy_addr(unsigned long vaddr);
void pcb_list_add(int list_name, struct pcb* target_pcb);
struct dequeue *alloc_dequeue();
void wait_list_update();
void idle_or_next_ready();