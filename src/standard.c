#include "standard.h"

#include "common.h"

#include <stdio.h>
#include <string.h>

static const MazenCStandard SUPPORTED_STANDARDS[] = {
    {"c89", MAZEN_STANDARD_GROUP_ISO},
    {"c99", MAZEN_STANDARD_GROUP_ISO},
    {"c11", MAZEN_STANDARD_GROUP_ISO},
    {"c17", MAZEN_STANDARD_GROUP_ISO},
    {"c23", MAZEN_STANDARD_GROUP_ISO},
    {"gnu89", MAZEN_STANDARD_GROUP_GNU},
    {"gnu99", MAZEN_STANDARD_GROUP_GNU},
    {"gnu11", MAZEN_STANDARD_GROUP_GNU},
    {"gnu17", MAZEN_STANDARD_GROUP_GNU},
    {"gnu23", MAZEN_STANDARD_GROUP_GNU},
    {"c2y", MAZEN_STANDARD_GROUP_EXPERIMENTAL},
    {"gnu2y", MAZEN_STANDARD_GROUP_EXPERIMENTAL}
};

const MazenCStandard *mazen_standard_default(void) {
    return &SUPPORTED_STANDARDS[3];
}

const MazenCStandard *mazen_standard_find(const char *name) {
    size_t i;

    if (name == NULL) {
        return NULL;
    }

    for (i = 0; i < MAZEN_ARRAY_LEN(SUPPORTED_STANDARDS); ++i) {
        if (strcmp(SUPPORTED_STANDARDS[i].name, name) == 0) {
            return &SUPPORTED_STANDARDS[i];
        }
    }

    return NULL;
}

const MazenCStandard *mazen_standard_select(const char *cli_name, const char *config_name) {
    const MazenCStandard *standard;

    if (cli_name != NULL) {
        standard = mazen_standard_find(cli_name);
        if (standard != NULL) {
            return standard;
        }
    }
    if (config_name != NULL) {
        standard = mazen_standard_find(config_name);
        if (standard != NULL) {
            return standard;
        }
    }

    return mazen_standard_default();
}

char *mazen_standard_flag(const MazenCStandard *standard) {
    return mazen_format("-std=%s", standard != NULL ? standard->name : mazen_standard_default()->name);
}

void mazen_standard_print_supported(FILE *stream) {
    size_t i;

    fputs("Supported C standards:\n\n", stream);
    for (i = 0; i < MAZEN_ARRAY_LEN(SUPPORTED_STANDARDS); ++i) {
        if (SUPPORTED_STANDARDS[i].group != MAZEN_STANDARD_GROUP_ISO) {
            continue;
        }
        fprintf(stream, "%s\n", SUPPORTED_STANDARDS[i].name);
    }

    fputs("\nGNU dialects:\n\n", stream);
    for (i = 0; i < MAZEN_ARRAY_LEN(SUPPORTED_STANDARDS); ++i) {
        if (SUPPORTED_STANDARDS[i].group != MAZEN_STANDARD_GROUP_GNU) {
            continue;
        }
        fprintf(stream, "%s\n", SUPPORTED_STANDARDS[i].name);
    }

    fputs("\nExperimental:\n\n", stream);
    for (i = 0; i < MAZEN_ARRAY_LEN(SUPPORTED_STANDARDS); ++i) {
        if (SUPPORTED_STANDARDS[i].group != MAZEN_STANDARD_GROUP_EXPERIMENTAL) {
            continue;
        }
        fprintf(stream, "%s\n", SUPPORTED_STANDARDS[i].name);
    }
}
