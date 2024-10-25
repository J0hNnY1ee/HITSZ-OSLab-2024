#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);
static struct run *steal_memory_from_other_cpus(int cpu_id);

extern char end[];  // first address after kernel. defined by kernel.ld.

struct run {
  struct run *next;
};

// 每个CPU的内存链表和锁
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];  // 每个CPU独立的内存链表

// 初始化内存分配器
void kinit() {
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem[i].lock, "kmem");
  }
  freerange(end, (void *)PHYSTOP);
}

// 将物理地址范围内的页释放到当前CPU的链表
void freerange(void *pa_start, void *pa_end) {
  char *p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE) {
    kfree(p);
  }
}

// 释放内存页，加入当前CPU的链表
void kfree(void *pa) {
  struct run *r;
  int cpu_id = cpuid();  // 获取当前CPU核的ID

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  memset(pa, 1, PGSIZE);  // 填充垃圾数据以检测悬挂引用

  r = (struct run *)pa;

  acquire(&kmem[cpu_id].lock);
  r->next = kmem[cpu_id].freelist;
  kmem[cpu_id].freelist = r;
  release(&kmem[cpu_id].lock);
}

// 分配一页内存，如果当前链表为空，则尝试从其他CPU窃取内存
void *kalloc() {
  struct run *r;
  int cpu_id = cpuid();  // 获取当前CPU核的ID

  // 尝试从当前CPU的链表分配内存
  acquire(&kmem[cpu_id].lock);
  r = kmem[cpu_id].freelist;
  if (r) {
    kmem[cpu_id].freelist = r->next;
    release(&kmem[cpu_id].lock);
  } else {
    release(&kmem[cpu_id].lock);

    // 如果当前CPU链表为空，尝试从其他CPU窃取内存
    r = steal_memory_from_other_cpus(cpu_id);
  }

  if (r) {
    memset((char *)r, 5, PGSIZE);  // 填充垃圾数据
  }
  return (void *)r;
}

// 从其他CPU的链表窃取内存块
static struct run *steal_memory_from_other_cpus(int cpu_id) {
  struct run *r = 0;

  for (int i = 0; i < NCPU; i++) {
    if (i == cpu_id) continue;  // 跳过当前CPU

    acquire(&kmem[i].lock);
    if (kmem[i].freelist) {
      // 从其他CPU的链表中窃取一个块
      r = kmem[i].freelist;
      kmem[i].freelist = r->next;
      release(&kmem[i].lock);
      break;
    }
    release(&kmem[i].lock);
  }

  return r;
}
