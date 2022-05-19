#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[]) {
    int p1[2], p2[2];
    char buffer[5];
    pipe(p1);
    pipe(p2);
    

    if(fork() == 0) {
        close(p1[1]);
        close(p2[0]);
        
        read(p1[0], buffer, 4);
        buffer[4] = '\0';
        fprintf(1, "%d: received %s\n", getpid(), buffer);
        write(p2[1], "pong", 4);
        
        close(p1[1]);
        close(p2[0]);
        exit(0);
    }
    
    close(p1[0]);
    close(p2[1]);
    
    write(p1[1], "ping", 4);
    read(p2[0], buffer, 4);
    fprintf(1, "%d: received %s\n", getpid(), buffer);
    wait(0);

    close(p1[1]);
    close(p2[0]);
    exit(0);
}
