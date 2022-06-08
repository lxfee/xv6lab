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
  struct buf buf[NBUF];
  struct buckethead buckets[BUCKETSSIZE];
} bcache;


// must hold bucket lock
static void erase(struct buf* b) {
  struct bucketnode *bnode = &b->bnode;
  bnode->prev->next = bnode->next;
  bnode->next->prev = bnode->prev;
}

// must hold bucket lock, except init
static void insert(struct buf* b) {
  uint key = b->blockno % BUCKETSSIZE;
  struct buckethead *bhead = &bcache.buckets[key];
  struct bucketnode *bnode = &b->bnode;
  bnode->next = bhead->head.next;
  bnode->next->prev = bnode;
  bnode->prev = &bhead->head;
  bhead->head.next = bnode;
}


void
binit(void)
{
  struct buf *b;
  struct buckethead *bhead;

  for(bhead = &bcache.buckets[0]; bhead < &bcache.buckets[0] + BUCKETSSIZE; bhead++) {
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
  int key;

  key = blockno % BUCKETSSIZE;
  bhead = &bcache.buckets[key];

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
  
  b = 0;
  bhead = 0;
  for(struct buckethead * tbhead = &bcache.buckets[0]; tbhead < &bcache.buckets[0]+BUCKETSSIZE; tbhead++) {
    acquire(&tbhead->lock);
    int found = 0;
    for(struct bucketnode *bnode = tbhead->head.next; bnode != &tbhead->head; bnode = bnode->next) {
      struct buf *tb = bnode->buf;
      if(tb->refcnt == 0 && (b == 0 || tb->timestamp < b->timestamp)) {
        b = tb;
        if(!found) {
          if(bhead) {
            release(&bhead->lock);
          }
          bhead = tbhead;
          found = 1;
        }
      }
    }
    if(!found)
      release(&tbhead->lock);
  }

  if(b == 0) {
    panic("bget: no buffers");
  }
  // now holds b bucket lock
  erase(b);
  release(&bhead->lock);

  // now b is free
  b->dev = dev;
  b->blockno = blockno;
  b->valid = 0;
  b->refcnt = 1;

  bhead = &bcache.buckets[key];
  acquire(&bhead->lock);
  insert(b);
  release(&bhead->lock);
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
  uint key = b->blockno % BUCKETSSIZE;
  struct buckethead *bhead;
  bhead = &bcache.buckets[key];
  acquire(&bhead->lock);
  acquire(&tickslock);
  b->timestamp = ticks;
  release(&tickslock);
  b->refcnt--;
  release(&bhead->lock);
}

void
bpin(struct buf *b) {
  // 因为在这里mref一定不为0，所以b->blockno一定不会被修改，所以是安全的
  uint key = b->blockno % BUCKETSSIZE;
  struct buckethead *bhead;
  bhead = &bcache.buckets[key];
  acquire(&bhead->lock);
  b->refcnt++;
  release(&bhead->lock);

  // acquire(&bcache.lock);
  // b->refcnt++;
  // release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  // 因为在这里mref一定不为0，所以b->blockno一定不会被修改，所以是安全的
  uint key = b->blockno % BUCKETSSIZE;
  struct buckethead *bhead;
  bhead = &bcache.buckets[key];
  acquire(&bhead->lock);
  b->refcnt--;
  release(&bhead->lock);

  // acquire(&bcache.lock);
  // b->refcnt--;
  // release(&bcache.lock);
}


