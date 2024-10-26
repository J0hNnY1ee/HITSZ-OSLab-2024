#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"


void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel, defined by kernel.ld.

// 结构体定义：表示一个内存块
struct run {
  struct run *next; // 指向下一个空闲块的指针
};

// 每个 CPU 的内存管理结构
struct {
  struct spinlock lock; // 自旋锁，保护此 CPU 的内存链表
  struct run *freelist; // 此 CPU 的空闲内存链表头指针
} kmem[NCPU]; // 数组，存储每个 CPU 的内存管理结构

// 初始化内存分配器
void kinit() {
  for (int i = 0; i < NCPU; i++) {
    initlock(&kmem[i].lock, "kmem"); // 初始化每个 CPU 的自旋锁
    kmem[i].freelist = 0; // 初始化空闲链表为空
  }
  freerange(end, (void*)PHYSTOP); // 设置可用内存范围
}

// 设置可用的内存范围，逐页释放内存
void freerange(void *pa_start, void *pa_end) {
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p); // 调用kfree释放页面
}

// 释放指向的物理内存页面v
void kfree(void *pa) {
  struct run *r;

  // 检查地址是否有效
  if (((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  memset(pa, 1, PGSIZE); // 填充内存以捕获悬空引用

  r = (struct run*)pa; // 将地址转换为run结构

  int cpu_id = cpuid(); // 获取当前 CPU ID
  acquire(&kmem[cpu_id].lock); // 获取当前 CPU 的自旋锁
  r->next = kmem[cpu_id].freelist; // 将当前页面插入到本 CPU 的空闲链表
  kmem[cpu_id].freelist = r; // 更新链表头
  release(&kmem[cpu_id].lock); // 释放自旋锁
}

// 分配一页4096字节的物理内存。
// 如果当前 CPU 的链表不足，则尝试从其他链表窃取内存块。
void *kalloc(void) {
  struct run *r;

  int cpu_id = cpuid(); // 获取当前 CPU ID
  acquire(&kmem[cpu_id].lock); // 获取当前 CPU 的自旋锁

  // 尝试从当前 CPU 的链表获取内存块
  r = kmem[cpu_id].freelist; 
  if (r) {
    kmem[cpu_id].freelist = r->next; // 更新链表头，移除已分配的页面
    release(&kmem[cpu_id].lock); // 释放自旋锁
    memset((char*)r, 5, PGSIZE); // 将分配的内存填充为5（用于调试）
    return (void*)r; // 返回分配的内存指针
  }

  // 如果当前链表为空，则尝试从其他 CPU 的链表窃取内存
  release(&kmem[cpu_id].lock); // 释放当前 CPU 的自旋锁

  // 尝试窃取其他 CPU 的内存
  for (int i = 0; i < NCPU; i++) {
    if (i == cpu_id) continue; // 不要尝试从自己这里窃取
    acquire(&kmem[i].lock); // 获取其他 CPU 的自旋锁
    r = kmem[i].freelist; // 获取其他 CPU 的空闲链表头
    if (r) {
      kmem[i].freelist = r->next; // 移除已分配的页面
      release(&kmem[i].lock); // 释放其他 CPU 的自旋锁
      memset((char*)r, 5, PGSIZE); // 将分配的内存填充为5（用于调试）
      return (void*)r; // 返回窃取的内存指针
    }
    release(&kmem[i].lock); // 释放其他 CPU 的自旋锁
  }

  return 0; // 如果没有可用内存，返回0
}
