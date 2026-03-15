#include <stdio.h>

#include "app.h"

int main(void) {
    printf("%s => 4 + 5 = %d\n", app_name(), app_sum(4, 5));
    return 0;
}
