#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"



int
main(int argc, char *argv[]) {
    int p[2];
    pipe(p);
    
    if(fork() == 0) {
        int leftfd, rightfd, num, pri, n;

        loop:
        leftfd = p[0];
        rightfd = -1;
        close(p[1]);
        n = read(leftfd, &pri, 4);
        if(n > 0) {
            fprintf(1, "prime %d\n", pri);
            while(read(leftfd, &num, 4) > 0) {
                if(num % pri) {
                    if(rightfd < 0) {
                        pipe(p);
                        rightfd = p[1];
                        if(fork() == 0) {
                            goto loop;    
                        }
                    }
                    write(rightfd, &num, 4);
                }
            }
        }
        close(leftfd);
        if(rightfd >= 0) 
            close(rightfd);
        wait(0);
        exit(0);
    }

    close(p[0]);
    
    int i;
    for(i = 2; i < 35; i++) {
        write(p[1], &i, 4);
    }

    close(p[1]);
    wait(0);
    exit(0);
}
