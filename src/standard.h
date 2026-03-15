#ifndef MAZEN_STANDARD_H
#define MAZEN_STANDARD_H

#include <stdbool.h>
#include <stdio.h>

typedef enum {
    MAZEN_STANDARD_GROUP_ISO = 0,
    MAZEN_STANDARD_GROUP_GNU,
    MAZEN_STANDARD_GROUP_EXPERIMENTAL
} MazenStandardGroup;

typedef struct {
    const char *name;
    MazenStandardGroup group;
} MazenCStandard;

const MazenCStandard *mazen_standard_default(void);
const MazenCStandard *mazen_standard_find(const char *name);
const MazenCStandard *mazen_standard_select(const char *cli_name, const char *config_name);
char *mazen_standard_flag(const MazenCStandard *standard);
void mazen_standard_print_supported(FILE *stream);

#endif
