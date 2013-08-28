#include <type.h>
#include <list.h>
#include "pid.h"
#include <process.h>
#include <string.h>
#include <printk.h>
#include "elf.h"

extern unsigned long mm_pgd;
extern struct list_head *task_list;

void process_test() {
  struct file demo_1, demo_2;
  int pid_1, pid_2;

  demo_1.buf = (void *)0xc4040000;
  demo_1.size = 33807;
  demo_2.buf = (void *)0xc4080000;
  demo_2.size = 33807;

  pid_1 = create_process(&demo_1);
  printk(PR_SS_PROC, PR_LVL_INF, "process %d created\n", pid_1);

  pid_2 = create_process(&demo_2);
  printk(PR_SS_PROC, PR_LVL_INF, "process %d created\n", pid_2);
}

struct task_struct *init_kernel_task = NULL;

static struct task_struct *create_launch_kernel_task() {
  int pid;
  struct task_struct *task = NULL;
  pid = allocate_pid();
  if (pid < 0)
	return NULL;
  
  task = (struct task_struct *)kmalloc(sizeof(struct task_struct));

  if (NULL == task)
	return NULL;

  task->stack = (void *)&init_thread_union;

  task->pid = pid;

  printk(PR_SS_PROC, PR_LVL_DBG3, "%s process %d stack = %x\n", __func__, pid, task->stack);

  task->mm.pgd = mm_pgd;

  return task;
}

void initialize_process() {
  initialize_pid();
  schedule_initialize();
  init_kernel_task = create_launch_kernel_task();
}


void arm_start_kernel_thread(void) __asm__("arm_start_kernel_thread");

int create_kernel_thread(int (*fn)(void *)) {
  int pid;
  struct task_struct *task = NULL;
  struct pt_regs *regs = NULL;
  struct thread_info *thread = NULL;

  pid = allocate_pid();
  if (pid < 0)
	return pid;
  
  /* make task_struct */
  task = (struct task_struct *)kmalloc(sizeof(struct task_struct));

  if (NULL == task)
	return -1;

  task->stack = (void *)kmalloc(PAGE_SIZE*2);

  task->pid = pid;

  printk(PR_SS_PROC, PR_LVL_DBG3, "%s process %d stack = %x\n", __func__, pid, task->stack);

  INIT_LIST_HEAD(&(task->mm.mmap.list));
  task->mm.pgd = (unsigned long)kmalloc(PAGE_SIZE * 4);
  memcpy((void *)task->mm.pgd, (void *)mm_pgd, PAGE_SIZE * 4);

  INIT_LIST_HEAD(&(task->sched_en.queue_entry));

  /* populate initial content of stack */
  regs = task_pt_regs(task);
  arm_create_kernel_thread(fn, NULL, regs);
  regs->ARM_r0 = 0;
  regs->ARM_sp = 0; /* this is user-space sp, don't need to set it */
	
  thread = task_thread_info(task);
  thread->task = task;
  thread->cpu_domain = arm_calc_kernel_domain();
  thread->tp_value = 0;
  thread->cpu_context.sp = (unsigned long)regs;
  thread->cpu_context.pc = (unsigned long)arm_start_kernel_thread;

  enqueue_task(task, sched_enqueue_flag_new);

  return pid;
}

int create_process(struct file *filep) {
  int pid;
  struct task_struct *task = NULL;
  pid = allocate_pid();
  if (pid < 0)
	return pid;
  
  task = (struct task_struct *)kmalloc(sizeof(struct task_struct));

  if (NULL == task)
	return -1;

  task->stack = (void *)kmalloc(PAGE_SIZE*2);

  task->pid = pid;

  printk(PR_SS_PROC, PR_LVL_DBG3, "%s process %d stack = %x\n", __func__, pid, task->stack);

  INIT_LIST_HEAD(&(task->mm.mmap.list));
  task->mm.pgd = (unsigned long)kmalloc(PAGE_SIZE * 4);
  memcpy((void *)task->mm.pgd, (void *)mm_pgd, PAGE_SIZE * 4);

  INIT_LIST_HEAD(&(task->sched_en.queue_entry));

  load_elf_binary(filep, &task->regs, &task->mm);

  enqueue_task(task, sched_enqueue_flag_new);

  return pid;
}

void destroy_process(struct task_struct *task) {
  free_pid(task->pid);
  kfree(task);
}
