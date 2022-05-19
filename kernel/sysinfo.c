#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "elf.h"
#include "sysinfo.h"

int sysinfo(uint64 addr) {
    struct sysinfo info;
    info.freemem = freemem();
    info.nproc = procnum();
    struct proc* p = myproc();
    if(!p) return -1;
    if(copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0)
      return -1;
    return 0;
}

