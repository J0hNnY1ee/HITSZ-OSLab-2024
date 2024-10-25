#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"


extern struct proc proc[];
uint64 sys_yield(void) {
    struct proc *p = mycpu()->proc;  // Get the current process running on the CPU

    // Print the memory region where the context of the current process is saved
    printf("Save the context of the process to the memory region from address %p to %p\n", 
           (uint64)&p->context, (uint64)&p->context + sizeof(struct context));

    // Print the PID and user PC of the current running process
    printf("Current running process pid is %d and user pc is %p\n", 
           p->pid, p->trapframe->epc);

    // Use a for loop to search for the next runnable process
    int current_index = p - proc;  // Calculate the index of the current process
    for (int i = (current_index + 1) % NPROC; i != current_index; i = (i + 1) % NPROC) {
        if (proc[i].state == RUNNABLE) {  // Check if the process is runnable
            // Print the PID and user PC of the next runnable process
            printf("Next runnable process pid is %d and user pc is %p\n", 
                   proc[i].pid, proc[i].trapframe->epc);
            break;  // Exit the loop once a runnable process is found
        }
    }

    yield();  // Yield the CPU and switch context to another process
    return 0;  // Return 0 to indicate success
}


uint64 sys_exit(void) {
  int n;
  if (argint(0, &n) < 0) return -1;
  exit(n);
  return 0;  // not reached
}

uint64 sys_getpid(void) { return myproc()->pid; }

uint64 sys_fork(void) { return fork(); }

uint64 sys_wait(void) {
  uint64 p;
  int flag;
  if (argaddr(0, &p) < 0) return -1;
  if (argint(1, &flag) < 0) return -1;
  return wait(p,flag);
}

uint64 sys_sbrk(void) {
  int addr;
  int n;

  if (argint(0, &n) < 0) return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0) return -1;
  return addr;
}

uint64 sys_sleep(void) {
  int n;
  uint ticks0;

  if (argint(0, &n) < 0) return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n) {
    if (myproc()->killed) {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64 sys_kill(void) {
  int pid;

  if (argint(0, &pid) < 0) return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64 sys_uptime(void) {
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64 sys_rename(void) {
  char name[16];
  int len = argstr(0, name, MAXPATH);
  if (len < 0) {
    return -1;
  }
  struct proc *p = myproc();
  memmove(p->name, name, len);
  p->name[len] = '\0';
  return 0;
}
