// 物理内存分配器，用于用户进程、内核栈、页表页和管道缓冲区。
// 该分配器按 4096 字节（一个页）的单位分配内存。

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void free_memory_range(void *start_addr, void *end_addr);

extern char end[]; // 内核结束后的第一个地址，由 kernel.ld 定义。

// run 结构体用于描述一个空闲的内存块
struct run {
  struct run *next; // 指向下一个空闲块的指针
};

// kmem 结构体，用于管理一个内核内存分配链表
struct {
  struct spinlock lock;    // 自旋锁，保护链表的并发访问
  struct run *freelist;    // 指向空闲内存块链表的头指针
} kmem;

// 每个 CPU 拥有一个独立的 kmems 分配链表，用于减少锁竞争
struct {
  struct spinlock lock;    // 自旋锁，保护每个 CPU 的链表
  struct run *freelist;    // 指向该 CPU 空闲内存块的链表头指针
} kmems[NCPU];

// 初始化内存分配器
void
kinit()
{ 
  // 初始化每个 CPU 的自旋锁
  for(int i = 0; i < NCPU; i++) {
    initlock(&kmems[i].lock, "kmem");
  }
  // 初始化空闲内存块链表，范围从 `end` 到物理内存上限 PHYSTOP
  free_memory_range(end, (void*)PHYSTOP);
}

// 将指定范围内的物理地址（起始地址到结束地址）划分为 4096 字节的块并释放
void
free_memory_range(void *start_addr, void *end_addr)
{
  char *page_addr;
  // 将起始地址向上对齐到页边界
  page_addr = (char*)PGROUNDUP((uint64)start_addr);
  // 遍历所有 4096 字节的页，并将它们逐一释放
  for(; page_addr + PGSIZE <= (char*)end_addr; page_addr += PGSIZE)
    kfree(page_addr);  // 释放页
}

// 释放由 pa 指向的物理内存页
// 通常，应该是由 kalloc() 分配的内存，
// 但在初始化时，分配器可以直接调用 kfree()
void
kfree(void *page_addr)
{
  struct run *free_block;

  // 检查地址有效性，确保它是页对齐的、在内核结束后并且在物理内存上限之内
  if(((uint64)page_addr % PGSIZE) != 0 || (char*)page_addr < end || (uint64)page_addr >= PHYSTOP)
    panic("kfree");

  // 将释放的内存填充为 1，方便调试时检测悬空引用
  memset(page_addr, 1, PGSIZE);

  free_block = (struct run*)page_addr;

  // 获取当前 CPU ID，并关闭中断防止调度切换
  push_off();
  int current_cpu = cpuid();
  // 锁住当前 CPU 的空闲链表，防止并发修改
  acquire(&kmems[current_cpu].lock);
  // 将释放的块添加到当前 CPU 的空闲链表头
  free_block->next = kmems[current_cpu].freelist;
  kmems[current_cpu].freelist = free_block;
  // 释放锁
  release(&kmems[current_cpu].lock);
  // 恢复中断状态
  pop_off();
}

// 分配一个 4096 字节大小的物理内存页
// 返回指向该内存页的指针，内核可以使用该地址
// 如果分配失败，则返回 0
void *
kalloc(void)
{
  struct run *allocated_block;

  // 获取当前 CPU ID，并关闭中断以防止切换
  push_off();
  int current_cpu = cpuid();
  // 锁住当前 CPU 的空闲链表
  acquire(&kmems[current_cpu].lock);
  // 从当前 CPU 的空闲链表中获取一个空闲块
  allocated_block = kmems[current_cpu].freelist;
  if(allocated_block)
    kmems[current_cpu].freelist = allocated_block->next;  // 更新链表头指针
  release(&kmems[current_cpu].lock);

  // 如果当前 CPU 没有空闲块，从其他 CPU 的链表中尝试分配
  if(!allocated_block) {
    for(int i = 0; i < NCPU; i++) {
      if(i == current_cpu) continue;  // 跳过当前 CPU
      acquire(&kmems[i].lock);
      allocated_block = kmems[i].freelist;
      if(allocated_block) {
        kmems[i].freelist = allocated_block->next;  // 更新链表头指针
        release(&kmems[i].lock);
        break;
      }
      release(&kmems[i].lock);
    }
  }
  // 恢复中断状态
  pop_off();

  // 如果成功分配到内存，则将内存填充为 5，用于调试
  if(allocated_block)
    memset((char*)allocated_block, 5, PGSIZE); // 填充内存
  return (void*)allocated_block;
}
