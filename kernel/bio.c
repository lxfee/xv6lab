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

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct buckethead buckets[BUCKETSSIZE];
} bcache;

static inline uint idx(uint blockno) {
  return blockno % BUCKETSSIZE;
}

static void erase(struct buf* b) {
  struct bucketnode *bnode = &b->bnode;
  bnode->prev->next = bnode->next;
  bnode->next->prev = bnode->prev;
}

static void insert(struct buf* b) {
  struct bucketnode *bhead = &bcache.buckets[idx(b->blockno)].head;
  struct bucketnode *bnode = &b->bnode;
  bnode->next = bhead->next;
  bnode->prev = bhead;
  bhead->next->prev = bnode;
  bhead->next = bnode;
}


void
binit(void)
{
  struct buf *b;
  struct buckethead *bhead;

  initlock(&bcache.lock, "bcache");
  for(bhead = bcache.buckets; bhead < bcache.buckets+BUCKETSSIZE; bhead++) {
    bhead->head.next = &bhead->head;
    bhead->head.prev = &bhead->head;
    initlock(&bhead->lock, "bcache.bucket");
  }

  // Create linked list of buffers
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->bnode.buf = b;
    insert(b);
    initsleeplock(&b->lock, "buffer");
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buckethead *bhead;
  struct bucketnode *bnode;
  struct buf *b;

  bhead = &bcache.buckets[idx(blockno)];

  acquire(&bhead->lock);
  // Is the block already cached?
  for(bnode = bhead->head.next; bnode != &bhead->head; bnode = bnode->next){
    b = bnode->buf;
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bhead->lock);
      acquiresleep(&b->lock); 
      return b;
    }
  }  
  release(&bhead->lock);

  acquire(&bcache.lock);  
  uint mintimestamp = -1;
  b = 0;
  struct buckethead *tbhead = 0;
  for(struct buckethead *nbhead = bcache.buckets; nbhead < bcache.buckets+BUCKETSSIZE; nbhead++) {
    acquire(&nbhead->lock);
    int flag = 0;
    for(struct bucketnode *bnode = nbhead->head.next; bnode != &nbhead->head; bnode = bnode->next) {
      struct buf *tb = bnode->buf;
      if(tb->refcnt == 0 && (tb->timestamp < mintimestamp)) {
        b = tb;
        mintimestamp = tb->timestamp;
        flag = 1;
      }
    }
    if(flag) {
      if(tbhead) release(&tbhead->lock);
      tbhead = nbhead;
    } else {
      release(&nbhead->lock);
    }
  }

  if(b == 0) {
    panic("bget: no buffers");
  }
  
  if(tbhead != bhead) {
    erase(b);
    release(&tbhead->lock);
  }

  // now b is free
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;

  if(tbhead != bhead) {
    acquire(&bhead->lock);
    insert(b);
  }

  release(&bhead->lock);
  
  release(&bcache.lock);
  acquiresleep(&b->lock);
  return b;
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  releasesleep(&b->lock);

  // 因为在这里mref一定不为0，所以b->blockno一定不会被修改，所以是安全的
  struct buckethead *bhead = &bcache.buckets[idx(b->blockno)];
  acquire(&bhead->lock);  

  b->refcnt--;
  acquire(&tickslock);
  b->timestamp = ticks;
  release(&tickslock);
  
  release(&bhead->lock);
}

void
bpin(struct buf *b) {
  struct buckethead *bhead = &bcache.buckets[idx(b->blockno)];
  acquire(&bhead->lock);
  b->refcnt++;
  release(&bhead->lock);
}

void
bunpin(struct buf *b) {
  struct buckethead *bhead = &bcache.buckets[idx(b->blockno)];
  acquire(&bhead->lock);
  b->refcnt--;
  release(&bhead->lock);
}


