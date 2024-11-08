// 用于磁盘块的缓冲区缓存

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKETS 13  // 定义哈希桶的数量

// 缓冲区缓存结构体，包含每个桶的锁、缓冲区数组、桶头节点和空闲列表
struct bcache {
  struct spinlock lock[NBUCKETS];     // 每个桶的锁
  struct buf buf[NBUF];               // 缓冲区数组
  struct buf bucket[NBUCKETS];        // 每个哈希桶的链表头节点
  int freelist[NBUCKETS];             // 每个桶的空闲块数量
} bcache;


// 从链表中移除最近最少使用的节点（从链表尾部开始查找）
// 返回找到的空闲节点，如果没有找到空闲节点，则返回 0
static struct buf*
remove_lru(struct buf *head)
{
  struct buf *node;
  for(node = head->prev; node != head; node = node->prev) {
    if(node->refcnt == 0) {  // 找到空闲块
      node->next->prev = node->prev;
      node->prev->next = node->next;
      return node;
    }
  }
  return 0;
}


// 选择拥有最多空闲缓冲块的桶
static int
find_richest_bucket(void)
{
  int max_free = 0;
  int selected_bucket = -1;
  
  for(int i = 0; i < NBUCKETS; i++) {
    if(bcache.freelist[i] > max_free) {  // 找到空闲块最多的桶
      max_free = bcache.freelist[i];
      selected_bucket = i;
    }
  }
  
  return selected_bucket;  // 返回拥有最多空闲块的桶索引
}

static int
steal_buffers(int target_bucket, int donor_bucket)
{
  if(donor_bucket < 0) return 0;  // 如果没有富有桶，则返回 0
  
  acquire(&bcache.lock[donor_bucket]);
  int buffers_to_steal = bcache.freelist[donor_bucket] / 2;  // 计算要“偷”的块数量
  
  if(buffers_to_steal > 0) {
    bcache.freelist[target_bucket] += buffers_to_steal;
    bcache.freelist[donor_bucket] -= buffers_to_steal;
    
    for(int i = 0; i < buffers_to_steal; i++) {
      struct buf *stolen_buffer = remove_lru(&bcache.bucket[donor_bucket]);
      if(stolen_buffer)
        {
          stolen_buffer->next = &bcache.bucket[target_bucket];
          stolen_buffer->prev = bcache.bucket[target_bucket].prev;
          bcache.bucket[target_bucket].prev->next = stolen_buffer;
          bcache.bucket[target_bucket].prev = stolen_buffer; 
          } // 插入到目标桶的尾部
    }
  }
  
  release(&bcache.lock[donor_bucket]);
  return buffers_to_steal;
}

// 缓冲区缓存初始化
void
binit(void)
{
  struct buf *b;

  // 初始化每个桶的锁和链表
  for(int i = 0; i < NBUCKETS; i++) {
    initlock(&bcache.lock[i], "bcache");
    bcache.bucket[i].next = &bcache.bucket[i];
    bcache.bucket[i].prev = &bcache.bucket[i];
    bcache.freelist[i] = 0;
  }

  // 将缓存块均匀分配到各个桶中
  for(b = bcache.buf; b < bcache.buf + NBUF; b++) {
    int bucket_index = (b - bcache.buf) % NBUCKETS;
    initsleeplock(&b->lock, "buffer");
    b->next = bcache.bucket[bucket_index].next;
    b->prev = &bcache.bucket[bucket_index];
    bcache.bucket[bucket_index].next->prev = b;
    bcache.bucket[bucket_index].next = b;  // 将块插入对应桶的头部
    bcache.freelist[bucket_index]++;
  }
}

// 获取一个缓冲区
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *buffer;
  int target_bucket = blockno% NBUCKETS;
  
  acquire(&bcache.lock[target_bucket]);

  // 在目标桶中查找块
  for(buffer = bcache.bucket[target_bucket].next; buffer != &bcache.bucket[target_bucket]; buffer = buffer->next) {
    if(buffer->dev == dev && buffer->blockno == blockno) {  // 找到目标块
      buffer->refcnt++;
      bcache.freelist[target_bucket] -= (buffer->refcnt == 1);  // 减少空闲块数量
      release(&bcache.lock[target_bucket]);
      acquiresleep(&buffer->lock);
      return buffer;
    }
  }

  // 如果当前桶没有空闲块，从其他桶偷一半
  if(bcache.freelist[target_bucket] == 0) {
    int donor = find_richest_bucket();
    if(donor < 0 || steal_buffers(target_bucket, donor) == 0) {
      panic("bget: no buffers");
    }
  }

  // 分配一个空闲块
  for(buffer = bcache.bucket[target_bucket].prev; buffer != &bcache.bucket[target_bucket]; buffer = buffer->prev) {
    if(buffer->refcnt == 0) {  // 找到空闲块
      buffer->dev = dev;
      buffer->blockno = blockno;
      buffer->valid = 0;
      buffer->refcnt = 1;
      bcache.freelist[target_bucket]--;  // 更新空闲列表
      release(&bcache.lock[target_bucket]);
      acquiresleep(&buffer->lock);
      return buffer;
    }
  }

  panic("bget: no buffers");
}

// 读取指定的块内容
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);  // 如果无效，则从磁盘读取
    b->valid = 1;
  }
  return b;
}

// 将缓冲区内容写回磁盘
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);  // 写回磁盘
}

// 释放缓冲区
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  int bucket_index =  b->blockno % NBUCKETS;
  acquire(&bcache.lock[bucket_index]);
  
  b->refcnt--;  // 引用计数减 1
  if(b->refcnt == 0) {  // 如果没有进程引用该块
    // 将释放的块移到链表头部（最近使用）
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = bcache.bucket[bucket_index].next;
    b->prev = &bcache.bucket[bucket_index];
    bcache.bucket[bucket_index].next->prev = b;
    bcache.bucket[bucket_index].next = b;
    bcache.freelist[bucket_index]++;  // 增加空闲块数量
  }
  
  release(&bcache.lock[bucket_index]);
}

// 增加缓冲区的引用计数，防止被回收
void
bpin(struct buf *b)
{
  int bucket_index = b->blockno% NBUCKETS;
  acquire(&bcache.lock[bucket_index]);
  b->refcnt++;
  release(&bcache.lock[bucket_index]);
}

// 减少缓冲区的引用计数，允许其被回收
void
bunpin(struct buf *b)
{
  int bucket_index = b->blockno% NBUCKETS;
  acquire(&bcache.lock[bucket_index]);
  b->refcnt--;
  release(&bcache.lock[bucket_index]);
}
