#include <stdio.h>

#include "vendor/mini/log.h"

void mini_log(const char *message) {
    printf("[mini] %s\n", message);
}
