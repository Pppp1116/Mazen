#ifndef MAZEN_AUTOLIB_H
#define MAZEN_AUTOLIB_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

void autolib_infer(const char *root_dir, const StringList *source_paths, StringList *libs,
                  bool allow_low_confidence);
bool autolib_infer_from_linker_output(const char *output, StringList *libs, bool allow_low_confidence);
uint64_t autolib_source_hash(const char *root_dir, const StringList *source_paths);

#endif
