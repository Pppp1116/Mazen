#ifndef MAZEN_CLASSIFIER_H
#define MAZEN_CLASSIFIER_H

#include "common.h"
#include "config.h"
#include "diag.h"
#include "scanner.h"

bool classifier_analyze(const char *root_dir, const ScanResult *scan, const MazenConfig *config, ProjectInfo *project,
                        Diagnostic *diag);

#endif
