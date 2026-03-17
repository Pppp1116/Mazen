#ifndef MAZEN_DIAG_H
#define MAZEN_DIAG_H

#include "common.h"

typedef struct {
    char *message;
    char *hint;
} Diagnostic;

void diag_init(Diagnostic *diag);
void diag_clear(Diagnostic *diag);
void diag_free(Diagnostic *diag);
void diag_set(Diagnostic *diag, const char *fmt, ...) MAZEN_PRINTF_LIKE(2, 3);
void diag_set_hint(Diagnostic *diag, const char *fmt, ...) MAZEN_PRINTF_LIKE(2, 3);
void diag_print_error(const Diagnostic *diag);
void diag_info(const char *fmt, ...) MAZEN_PRINTF_LIKE(1, 2);
void diag_note(const char *fmt, ...) MAZEN_PRINTF_LIKE(1, 2);
void diag_warn(const char *fmt, ...) MAZEN_PRINTF_LIKE(1, 2);

#endif
