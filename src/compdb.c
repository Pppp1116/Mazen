#include "compdb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void compdb_entry_free(CompDbEntry *entry) {
  free(entry->directory);
  free(entry->file);
  free(entry->command);
  free(entry->output);
  entry->directory = NULL;
  entry->file = NULL;
  entry->command = NULL;
  entry->output = NULL;
}

static char *json_escape(const char *text) {
  String escaped;
  size_t i;

  string_init(&escaped);
  for (i = 0; text[i] != '\0'; ++i) {
    switch (text[i]) {
    case '\\':
      string_append(&escaped, "\\\\");
      break;
    case '"':
      string_append(&escaped, "\\\"");
      break;
    case '\n':
      string_append(&escaped, "\\n");
      break;
    case '\r':
      string_append(&escaped, "\\r");
      break;
    case '\t':
      string_append(&escaped, "\\t");
      break;
    default:
      string_append_char(&escaped, text[i]);
      break;
    }
  }
  return string_take(&escaped);
}

void compdb_entry_list_init(CompDbEntryList *list) {
  list->items = NULL;
  list->len = 0;
  list->cap = 0;
}

void compdb_entry_list_free(CompDbEntryList *list) {
  size_t i;

  for (i = 0; i < list->len; ++i) {
    compdb_entry_free(&list->items[i]);
  }
  free(list->items);
  list->items = NULL;
  list->len = 0;
  list->cap = 0;
}

CompDbEntry *compdb_entry_list_push(CompDbEntryList *list) {
  size_t cap;
  CompDbEntry *entry;

  if (list->len == list->cap) {
    cap = list->cap == 0 ? 8 : list->cap * 2;
    list->items = mazen_xrealloc(list->items, cap * sizeof(*list->items));
    list->cap = cap;
  }

  entry = &list->items[list->len++];
  entry->directory = NULL;
  entry->file = NULL;
  entry->command = NULL;
  entry->output = NULL;
  return entry;
}

bool compdb_write(const char *path, const CompDbEntryList *list,
                  Diagnostic *diag) {
  String json;
  size_t i;

  string_init(&json);
  string_append(&json, "[\n");
  for (i = 0; i < list->len; ++i) {
    char *directory = json_escape(
        list->items[i].directory != NULL ? list->items[i].directory : "");
    char *file =
        json_escape(list->items[i].file != NULL ? list->items[i].file : "");
    char *command = json_escape(
        list->items[i].command != NULL ? list->items[i].command : "");
    char *output =
        json_escape(list->items[i].output != NULL ? list->items[i].output : "");

    string_appendf(&json,
                   "  "
                   "{\"directory\":\"%s\",\"file\":\"%s\",\"command\":\"%s\","
                   "\"output\":\"%s\"}%s\n",
                   directory, file, command, output,
                   i + 1 == list->len ? "" : ",");
    free(directory);
    free(file);
    free(command);
    free(output);
  }
  string_append(&json, "]\n");

  if (!write_text_file(path, json.data == NULL ? "" : json.data, json.len)) {
    diag_set(diag, "failed to write compile database `%s`", path);
    string_free(&json);
    return false;
  }

  string_free(&json);
  return true;
}
