#ifndef MAZEN_AUTOLIB_H
#define MAZEN_AUTOLIB_H

#include "common.h"

#include <stdbool.h>

void autolib_infer(const char *root_dir, const StringList *source_paths, StringList *libs);
bool autolib_infer_from_linker_output(const char *output, StringList *libs);

#endif
