## thread lab

### 实现

不难

模仿内核切换线程方式即可，.S文件甚至不用改。

由于切换上下文是以函数调用方式实现的，因此只要保存callee-saved的寄存器即可。还有sp寄存器，因为每个线程虽然是共享内存，但栈是独立的。



### 注意事项

1. `pthread_cond_wait(&bstate.barrier_cond, &bstate.barrier_mutex)`似乎并不会解锁bstate.barrier_mutex，不在后面加一个`unlock(&bstate.barrier_mutex)`，会死锁，不知道为什么。

2. 注意栈是**向下增长**的，所以设置线程sp指针的时候记得设置到栈的高地址。`t->context.sp = (uint64) (t->stack + STACK_SIZE)`


