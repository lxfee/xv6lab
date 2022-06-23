# mmap

挺简单，注意uvmunmap不要少了，否则fork_test会出现panic: free walk。