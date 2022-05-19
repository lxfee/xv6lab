#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

char* eargv[MAXARG + 1];
char buffer[512];

int main(int argc, char *argv[]) {
    if(argc < 2) exit(0);
    for(int i = 0; i < argc; i++) {
        eargv[i] = argv[i + 1];
    }
    eargv[argc - 1] = buffer;

    char ch;
    int n = 0;
    do {
        int cnt = 0;
        while((n = read(0, &ch, 1)) > 0) {
            if(ch == '\n') break;
            if(cnt + 1 < 512) {
                buffer[cnt++] = ch;
            }
        }
        buffer[cnt] = 0;
        if(n <= 0) break;

        if(fork() == 0) {
            exec(eargv[0], eargv);
            fprintf(2, "exec %s failed\n", argv[1]);
        }
        wait(0);
    } while(1);

    if(n < 0) exit(1);
    exit(0);
}
