#include "types.h"
#include "user.h"

int main(int argc, char *argv[])
{
    int pid;
    if (argc > 1) {
        pid = atoi(argv[1]);
    } else {
        pid = getpid();
    }
    dumppagetable(pid);
    exit();
}
