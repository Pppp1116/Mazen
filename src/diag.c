#include "diag.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static void diag_set_formatted(char **slot, const char *fmt, va_list args) {
    free(*slot);
    *slot = mazen_vformat(fmt, args);
}

static void stream_print(FILE *stream, const char *prefix, const char *fmt, va_list args) {
    fputs(prefix, stream);
    vfprintf(stream, fmt, args);
    fputc('\n', stream);
    fflush(stream);
}

void diag_init(Diagnostic *diag) {
    diag->message = NULL;
    diag->hint = NULL;
}

void diag_clear(Diagnostic *diag) {
    free(diag->message);
    free(diag->hint);
    diag->message = NULL;
    diag->hint = NULL;
}

void diag_free(Diagnostic *diag) {
    diag_clear(diag);
}

void diag_set(Diagnostic *diag, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    diag_set_formatted(&diag->message, fmt, args);
    va_end(args);
}

void diag_set_hint(Diagnostic *diag, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    diag_set_formatted(&diag->hint, fmt, args);
    va_end(args);
}

void diag_print_error(const Diagnostic *diag) {
    if (diag->message != NULL) {
        fprintf(stderr, "error: %s\n", diag->message);
    }
    if (diag->hint != NULL) {
        fprintf(stderr, "hint: %s\n", diag->hint);
    }
    fflush(stderr);
}

void diag_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    stream_print(stdout, "", fmt, args);
    va_end(args);
}

void diag_note(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    stream_print(stdout, "note: ", fmt, args);
    va_end(args);
}

void diag_warn(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    stream_print(stderr, "warning: ", fmt, args);
    va_end(args);
}
