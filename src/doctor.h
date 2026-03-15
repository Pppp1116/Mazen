#ifndef MAZEN_DOCTOR_H
#define MAZEN_DOCTOR_H

#include "common.h"
#include "config.h"
#include "standard.h"
#include "target.h"

void doctor_print_project(const ProjectInfo *project, const MazenConfig *config, const char *compiler,
                          const MazenCStandard *standard, const ResolvedTarget *target, int jobs,
                          const char *compile_commands_path);
void doctor_print_resolved_targets(const ResolvedTargetList *targets);
void doctor_print_targets(const ProjectInfo *project);

#endif
