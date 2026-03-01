#include "core_sched.h"
#include <unistd.h>

int main() {
    init_daemon();

    while (true) {
        apply_core_optimizations();
        sleep(1800); // 1800 秒 = 30 分鐘
    }

    return 0;
}
