#ifndef MAZEN_DOCTOR_H
#define MAZEN_DOCTOR_H

#include "common.h"
#include "config.h"
#include "standard.h"

void doctor_print_project(const ProjectInfo *project, const MazenConfig *config, const char *compiler,
                          const MazenCStandard *standard);
void doctor_print_targets(const ProjectInfo *project);

#endif
