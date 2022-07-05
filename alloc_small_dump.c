#include "pagingtestlib.h"

int main() {
    setup();
    printf(1, COUNT_MSG, 5);
    for (int i = 0; i < 5; ++i) {
        int pid = fork();
        if (pid == 0) {
            test_allocation_no_fork(300 * 1024 * 1024, "300MB", "100MB", 5 * 1024 * 1024, 100 * 1024 * 1024, 150 * 1024 * 1024, 100 * 1024 * 1024, 0);
            exit();
        } else {
            wait();
        }
    }
    finish();
}
