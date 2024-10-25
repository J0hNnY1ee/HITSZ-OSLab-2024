// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"


// 定义哈希桶的数量
#define NHASH 17

// 缓冲区缓存结构体，包含缓冲区数组和哈希桶
struct {
  struct spinlock bucket_locks[NHASH];  // 每个哈希桶的自旋锁
  struct buf buf[NBUF];                 // 缓冲区数组
  struct buf *hash_heads[NHASH];        // 每个桶的链表头
} bcache;

// 初始化缓冲区缓存及哈希桶
void binit(void) {
  // 初始化每个哈希桶的锁和链表头
  for (int i = 0; i < NHASH; i++) {
    initlock(&bcache.bucket_locks[i], "bcache_bucket");
    bcache.hash_heads[i] = 0;
  }

  // 初始化所有缓冲区，并放入第一个哈希桶
  for (int i = 0; i < NBUF; i++) {
    struct buf *b = &bcache.buf[i];
    b->refcnt = 0;  // 初始化引用计数为0
    b->next = bcache.hash_heads[0];
    b->prev = 0;
    if (bcache.hash_heads[0] != 0) {
      bcache.hash_heads[0]->prev = b;
    }
    bcache.hash_heads[0] = b;
    initsleeplock(&b->lock, "buffer");
  }
}

// 哈希函数，基于设备号和块号计算桶索引
static int hash(uint dev, uint blockno) {
  return (dev ^ blockno) % NHASH;
}

// 释放缓冲区，并将其移至桶的链表头部
void brelse(struct buf *b) {
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);  // 释放缓冲区的睡眠锁

  int index = hash(b->dev, b->blockno);

  acquire(&bcache.bucket_locks[index]);  // 获取桶的锁
  if (b->refcnt <= 0)
    panic("brelse: refcnt underflow");

  b->refcnt--;  // 减少引用计数
  if (b->refcnt == 0) {  // 如果没有进程在使用该块
    // 将块移到桶的链表头部
    if (b->prev) b->prev->next = b->next;
    if (b->next) b->next->prev = b->prev;
    if (bcache.hash_heads[index] == b) bcache.hash_heads[index] = b->next;

    b->next = bcache.hash_heads[index];
    b->prev = 0;
    if (bcache.hash_heads[index] != 0) {
      bcache.hash_heads[index]->prev = b;
    }
    bcache.hash_heads[index] = b;
  }
  release(&bcache.bucket_locks[index]);  // 释放桶的锁
}

// 获取指定块的缓冲区，或者从其他桶获取未使用的缓冲区
static struct buf* bget(uint dev, uint blockno) {
  int index = hash(dev, blockno);  // 计算哈希桶索引
  struct buf *b;

  acquire(&bcache.bucket_locks[index]);  // 获取当前桶的锁

  // 查找块是否已缓存
  for (b = bcache.hash_heads[index]; b != 0; b = b->next) {
    if (b->dev == dev && b->blockno == blockno) {
      b->refcnt++;
      release(&bcache.bucket_locks[index]);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.bucket_locks[index]);  // 没有找到时释放锁

  // 查找未使用的缓冲区
  for (int i = 0; i < NHASH; i++) {
    acquire(&bcache.bucket_locks[i]);  // 获取其他桶的锁
    for (b = bcache.hash_heads[i]; b != 0; b = b->next) {
      if (b->refcnt == 0) {  // 找到未使用的块
        // 从旧桶中移除
        if (b->prev) b->prev->next = b->next;
        if (b->next) b->next->prev = b->prev;
        if (bcache.hash_heads[i] == b) bcache.hash_heads[i] = b->next;

        release(&bcache.bucket_locks[i]);  // 释放旧桶的锁

        // 将块移入目标桶
        acquire(&bcache.bucket_locks[index]);
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        b->next = bcache.hash_heads[index];
        b->prev = 0;
        if (bcache.hash_heads[index] != 0) {
          bcache.hash_heads[index]->prev = b;
        }
        bcache.hash_heads[index] = b;
        release(&bcache.bucket_locks[index]);  // 释放目标桶的锁

        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&bcache.bucket_locks[i]);  // 释放其他桶的锁
  }

  panic("bget: no buffers available");
}

// 从磁盘读取块内容
struct buf* bread(uint dev, uint blockno) {
  struct buf *b = bget(dev, blockno);
  if (!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// 将缓冲区内容写回磁盘
void bwrite(struct buf *b) {
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// 固定缓冲区，防止其被回收
void bpin(struct buf *b) {
  int index = hash(b->dev, b->blockno);

  acquire(&bcache.bucket_locks[index]);
  b->refcnt++;
  release(&bcache.bucket_locks[index]);
}

// 取消固定缓冲区
void bunpin(struct buf *b) {
  int index = hash(b->dev, b->blockno);

  acquire(&bcache.bucket_locks[index]);
  b->refcnt--;
  release(&bcache.bucket_locks[index]);
}
