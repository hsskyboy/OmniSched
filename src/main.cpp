#include "core_sched.h"
#include <unistd.h>

int main() {
    init_daemon();

    while (true) {
        apply_core_optimizations();
        sleep(900); // 900 秒 = 15 分鐘
    }

    return 0;
}
