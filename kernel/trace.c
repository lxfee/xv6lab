#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "elf.h"

int trace(int mask) {
    struct proc* p = myproc();
    if(!p) return -1;
    p->tracemask = mask;
    return 0;
}