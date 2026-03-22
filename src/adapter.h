#ifndef MAZEN_ADAPTER_H
#define MAZEN_ADAPTER_H

#include "cli.h"
#include "config.h"
#include "diag.h"

typedef enum {
    MAZEN_PROJECT_ADAPTER_NONE = 0,
    MAZEN_PROJECT_ADAPTER_LINUX_KERNEL
} MazenProjectAdapterKind;

typedef struct {
    MazenProjectAdapterKind kind;
} MazenProjectAdapter;

void mazen_project_adapter_init(MazenProjectAdapter *adapter);
bool mazen_project_adapter_detect(const char *root_dir, const MazenConfig *config, MazenProjectAdapter *adapter);
bool mazen_project_adapter_is_active(const MazenProjectAdapter *adapter);
bool mazen_project_adapter_handle(const MazenProjectAdapter *adapter, const char *root_dir, const CliOptions *cli,
                                  Diagnostic *diag);

#endif
