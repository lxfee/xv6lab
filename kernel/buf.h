
struct bucketnode {
  struct bucketnode *next;
  struct bucketnode *prev;
  struct buf *buf;
};

struct buckethead {
  struct spinlock lock;
  struct bucketnode head;
};

struct buf {
  int valid;   // has data been read from disk?
  int disk;    // does disk "own" buf?
  uint dev;
  uint blockno;
  uint timestamp;
  uint refcnt;
  struct bucketnode bnode; 
  struct sleeplock lock;
  uchar data[BSIZE];
};



