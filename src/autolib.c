#include "autolib.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *lib;
    const char *const *headers;
    size_t header_count;
    const char *const *identifiers;
    size_t identifier_count;
    const char *const *prefixes;
    size_t prefix_count;
} AutoLibRule;

static const char *M_HEADERS[] = {"math.h"};
static const char *M_IDENTIFIERS[] = {
    "sqrt", "sqrtf", "sqrtl", "sin", "sinf", "sinl", "cos", "cosf", "cosl", "tan", "tanf", "tanl",
    "pow", "powf", "powl", "floor", "floorf", "ceil", "ceilf", "round", "roundf", "exp", "expf",
    "log", "logf", "log10", "log10f", "fmod", "fmodf", "atan2", "atan2f", "hypot", "hypotf", "cbrt", "cbrtf"
};

static const char *PTHREAD_HEADERS[] = {"pthread.h", "semaphore.h"};
static const char *PTHREAD_IDENTIFIERS[] = {"pthread_create", "pthread_join", "sem_init", "sem_wait", "sem_post"};
static const char *PTHREAD_PREFIXES[] = {"pthread_mutex_", "pthread_cond_", "pthread_rwlock_", "pthread_spin_",
                                         "pthread_barrier_", "sem_"};

static const char *DL_HEADERS[] = {"dlfcn.h"};
static const char *DL_IDENTIFIERS[] = {"dlopen", "dlsym", "dlclose", "dlerror", "dladdr"};

static const char *RT_HEADERS[] = {"aio.h", "mqueue.h"};
static const char *RT_IDENTIFIERS[] = {"clock_gettime", "clock_settime", "timer_create", "timer_settime", "mq_open",
                                       "mq_send", "mq_receive", "mq_close", "shm_open", "aio_read", "aio_write"};

static const AutoLibRule AUTO_LIB_RULES[] = {
    {"m", M_HEADERS, MAZEN_ARRAY_LEN(M_HEADERS), M_IDENTIFIERS, MAZEN_ARRAY_LEN(M_IDENTIFIERS), NULL, 0},
    {"pthread", PTHREAD_HEADERS, MAZEN_ARRAY_LEN(PTHREAD_HEADERS), PTHREAD_IDENTIFIERS,
     MAZEN_ARRAY_LEN(PTHREAD_IDENTIFIERS), PTHREAD_PREFIXES, MAZEN_ARRAY_LEN(PTHREAD_PREFIXES)},
    {"dl", DL_HEADERS, MAZEN_ARRAY_LEN(DL_HEADERS), DL_IDENTIFIERS, MAZEN_ARRAY_LEN(DL_IDENTIFIERS), NULL, 0},
    {"rt", RT_HEADERS, MAZEN_ARRAY_LEN(RT_HEADERS), RT_IDENTIFIERS, MAZEN_ARRAY_LEN(RT_IDENTIFIERS), NULL, 0}
};

static bool is_identifier_char(char ch) {
    return isalnum((unsigned char) ch) || ch == '_';
}

static char *strip_comments_and_literals(const char *content) {
    String out;
    size_t i = 0;
    enum {
        STATE_NORMAL,
        STATE_SLASH,
        STATE_LINE_COMMENT,
        STATE_BLOCK_COMMENT,
        STATE_BLOCK_COMMENT_STAR,
        STATE_STRING,
        STATE_STRING_ESCAPE,
        STATE_CHAR,
        STATE_CHAR_ESCAPE
    } state = STATE_NORMAL;

    string_init(&out);
    while (content[i] != '\0') {
        char ch = content[i++];

        switch (state) {
        case STATE_NORMAL:
            if (ch == '/') {
                state = STATE_SLASH;
            } else if (ch == '"') {
                string_append_char(&out, ' ');
                state = STATE_STRING;
            } else if (ch == '\'') {
                string_append_char(&out, ' ');
                state = STATE_CHAR;
            } else {
                string_append_char(&out, ch);
            }
            break;
        case STATE_SLASH:
            if (ch == '/') {
                string_append_char(&out, ' ');
                string_append_char(&out, ' ');
                state = STATE_LINE_COMMENT;
            } else if (ch == '*') {
                string_append_char(&out, ' ');
                string_append_char(&out, ' ');
                state = STATE_BLOCK_COMMENT;
            } else {
                string_append_char(&out, '/');
                string_append_char(&out, ch);
                state = STATE_NORMAL;
            }
            break;
        case STATE_LINE_COMMENT:
            string_append_char(&out, ch == '\n' ? '\n' : ' ');
            if (ch == '\n') {
                state = STATE_NORMAL;
            }
            break;
        case STATE_BLOCK_COMMENT:
            if (ch == '*') {
                string_append_char(&out, ' ');
                state = STATE_BLOCK_COMMENT_STAR;
            } else if (ch == '\n') {
                string_append_char(&out, '\n');
            } else {
                string_append_char(&out, ' ');
            }
            break;
        case STATE_BLOCK_COMMENT_STAR:
            if (ch == '/') {
                string_append_char(&out, ' ');
                state = STATE_NORMAL;
            } else if (ch == '\n') {
                string_append_char(&out, '\n');
                state = STATE_BLOCK_COMMENT;
            } else if (ch == '*') {
                string_append_char(&out, ' ');
            } else {
                string_append_char(&out, ' ');
                state = STATE_BLOCK_COMMENT;
            }
            break;
        case STATE_STRING:
            if (ch == '\\') {
                string_append_char(&out, ' ');
                state = STATE_STRING_ESCAPE;
            } else if (ch == '"') {
                string_append_char(&out, ' ');
                state = STATE_NORMAL;
            } else {
                string_append_char(&out, ch == '\n' ? '\n' : ' ');
            }
            break;
        case STATE_STRING_ESCAPE:
            string_append_char(&out, ch == '\n' ? '\n' : ' ');
            state = STATE_STRING;
            break;
        case STATE_CHAR:
            if (ch == '\\') {
                string_append_char(&out, ' ');
                state = STATE_CHAR_ESCAPE;
            } else if (ch == '\'') {
                string_append_char(&out, ' ');
                state = STATE_NORMAL;
            } else {
                string_append_char(&out, ch == '\n' ? '\n' : ' ');
            }
            break;
        case STATE_CHAR_ESCAPE:
            string_append_char(&out, ch == '\n' ? '\n' : ' ');
            state = STATE_CHAR;
            break;
        }
    }

    if (state == STATE_SLASH) {
        string_append_char(&out, '/');
    }
    return string_take(&out);
}

static void collect_includes(const char *content, StringList *includes) {
    const char *cursor = content;

    while (*cursor != '\0') {
        while (*cursor != '\0' && *cursor != '#') {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }
        ++cursor;
        while (*cursor != '\0' && isspace((unsigned char) *cursor)) {
            ++cursor;
        }
        if (strncmp(cursor, "include", 7) != 0 || is_identifier_char(cursor[7])) {
            while (*cursor != '\0' && *cursor != '\n') {
                ++cursor;
            }
            continue;
        }
        cursor += 7;
        while (*cursor != '\0' && isspace((unsigned char) *cursor)) {
            ++cursor;
        }
        if (*cursor == '"' || *cursor == '<') {
            char terminator = *cursor == '"' ? '"' : '>';
            String include;

            string_init(&include);
            ++cursor;
            while (*cursor != '\0' && *cursor != terminator && *cursor != '\n') {
                string_append_char(&include, *cursor++);
            }
            if (*cursor == terminator) {
                string_list_push_unique_take(includes, string_take(&include));
            } else {
                string_free(&include);
            }
        }
        while (*cursor != '\0' && *cursor != '\n') {
            ++cursor;
        }
    }
}

static bool contains_identifier(const char *text, const char *identifier) {
    size_t len = strlen(identifier);
    const char *cursor = text;

    while ((cursor = strstr(cursor, identifier)) != NULL) {
        char before = cursor == text ? '\0' : cursor[-1];
        char after = cursor[len];
        if (!is_identifier_char(before) && !is_identifier_char(after)) {
            return true;
        }
        ++cursor;
    }
    return false;
}

static bool contains_identifier_prefix(const char *text, const char *prefix) {
    size_t len = strlen(prefix);
    const char *cursor = text;

    while ((cursor = strstr(cursor, prefix)) != NULL) {
        char before = cursor == text ? '\0' : cursor[-1];
        if (!is_identifier_char(before) && is_identifier_char(cursor[len])) {
            return true;
        }
        ++cursor;
    }
    return false;
}

static bool includes_header(const StringList *includes, const char *header) {
    size_t i;

    for (i = 0; i < includes->len; ++i) {
        if (strcmp(includes->items[i], header) == 0) {
            return true;
        }
    }
    return false;
}

static bool rule_matches(const AutoLibRule *rule, const StringList *includes, const char *sanitized) {
    size_t i;

    for (i = 0; i < rule->header_count; ++i) {
        if (includes_header(includes, rule->headers[i])) {
            return true;
        }
    }
    for (i = 0; i < rule->identifier_count; ++i) {
        if (contains_identifier(sanitized, rule->identifiers[i])) {
            return true;
        }
    }
    for (i = 0; i < rule->prefix_count; ++i) {
        if (contains_identifier_prefix(sanitized, rule->prefixes[i])) {
            return true;
        }
    }
    return false;
}

void autolib_infer(const char *root_dir, const StringList *source_paths, StringList *libs) {
    size_t i;

    for (i = 0; i < source_paths->len; ++i) {
        char *absolute = path_join(root_dir, source_paths->items[i]);
        char *content = read_text_file(absolute, NULL);

        free(absolute);
        if (content != NULL) {
            char *sanitized = strip_comments_and_literals(content);
            StringList includes;
            size_t rule_index;

            string_list_init(&includes);
            collect_includes(content, &includes);
            for (rule_index = 0; rule_index < MAZEN_ARRAY_LEN(AUTO_LIB_RULES); ++rule_index) {
                if (rule_matches(&AUTO_LIB_RULES[rule_index], &includes, sanitized)) {
                    string_list_push_unique(libs, AUTO_LIB_RULES[rule_index].lib);
                }
            }
            string_list_free(&includes);
            free(sanitized);
            free(content);
        }
    }
}

bool autolib_infer_from_linker_output(const char *output, StringList *libs) {
    size_t before = libs->len;

    if (strstr(output, "undefined reference to `sqrt") != NULL ||
        strstr(output, "undefined reference to `sin") != NULL ||
        strstr(output, "undefined reference to `cos") != NULL ||
        strstr(output, "undefined reference to `tan") != NULL ||
        strstr(output, "undefined reference to `pow") != NULL ||
        strstr(output, "undefined reference to `floor") != NULL ||
        strstr(output, "undefined reference to `ceil") != NULL ||
        strstr(output, "undefined reference to `round") != NULL ||
        strstr(output, "undefined reference to `log") != NULL ||
        strstr(output, "undefined reference to `exp") != NULL ||
        strstr(output, "undefined reference to `fmod") != NULL ||
        strstr(output, "undefined reference to `hypot") != NULL) {
        string_list_push_unique(libs, "m");
    }
    if (strstr(output, "undefined reference to `pthread_") != NULL ||
        strstr(output, "undefined reference to `sem_") != NULL) {
        string_list_push_unique(libs, "pthread");
    }
    if (strstr(output, "undefined reference to `dlopen") != NULL ||
        strstr(output, "undefined reference to `dlsym") != NULL ||
        strstr(output, "undefined reference to `dlclose") != NULL ||
        strstr(output, "undefined reference to `dlerror") != NULL) {
        string_list_push_unique(libs, "dl");
    }
    if (strstr(output, "undefined reference to `clock_gettime") != NULL ||
        strstr(output, "undefined reference to `timer_") != NULL ||
        strstr(output, "undefined reference to `mq_") != NULL ||
        strstr(output, "undefined reference to `shm_open") != NULL) {
        string_list_push_unique(libs, "rt");
    }

    return libs->len > before;
}
