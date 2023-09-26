#include "../../oskernel/lib/stdio.h"
#include "../../oskernel/lib/user/syscall.h"
#include "../../oskernel/lib/string.h"

int main(int argc, const char **argv)
{
    int arg_idx = 0;

    while (arg_idx < argc) {
        printf("argv[%d] is %s\n", arg_idx, argv[arg_idx]);
        arg_idx++;
    }

    int pid = fork();
    if (pid) {
        int delay = 900000;
        while (delay--);
        printf("    I'm father prog, my pid %d, I will show process list.\n", getpid());
        ps();
    } else {
        char abs_path[512] = {0};
        printf("    I'm child prog, my pid %d, I will exec %s right now.\n", getpid(), argv[1]);
        if (argv[1][0] != '/') {
            getcwd(abs_path, 512);
            strcat(abs_path, "/");
            strcat(abs_path, argv[1]);
            execv(abs_path, argv);
        } else {
            execv(argv[1], argv);
        }
    }
    while (1);
    return 0;
}
