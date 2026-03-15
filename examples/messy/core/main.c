#include <stdio.h>

#include "core/math.h"
#include "vendor/mini/log.h"

int main(void) {
    int value = math_add(7, 5);
    mini_log("running messy example");
    printf("messy example: 7 + 5 = %d\n", value);
    return 0;
}
