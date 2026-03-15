#include "cli.h"

#include "mazen.h"
#include "standard.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  const char *name;
  MazenCommand command;
} CommandEntry;

static const CommandEntry COMMANDS[] = {
    {"build", MAZEN_CMD_BUILD},         {"run", MAZEN_CMD_RUN},
    {"release", MAZEN_CMD_RELEASE},     {"clean", MAZEN_CMD_CLEAN},
    {"doctor", MAZEN_CMD_DOCTOR},       {"list", MAZEN_CMD_LIST},
    {"standards", MAZEN_CMD_STANDARDS}, {"help", MAZEN_CMD_HELP}};

static bool validate_standard_value(const char *value, const char *option_name,
                                    Diagnostic *diag) {
  if (mazen_standard_find(value) != NULL) {
    return true;
  }

  diag_set(diag, "unsupported C standard `%s` for %s", value, option_name);
  diag_set_hint(diag, "Run `mazen standards` to list supported values");
  return false;
}

static bool parse_job_count_value(const char *value, const char *option_name,
                                  int *jobs_out, Diagnostic *diag) {
  char *end = NULL;
  long jobs;

  errno = 0;

  if (value == NULL || *value == '\0') {
    diag_set(diag, "%s requires a positive integer value", option_name);
    return false;
  }

  jobs = strtol(value, &end, 10);
  if (*end != '\0' || jobs <= 0 || jobs > INT_MAX || errno == ERANGE) {
    diag_set(diag, "%s requires a positive integer value", option_name);
    return false;
  }

  *jobs_out = (int)jobs;
  return true;
}

static bool command_from_text(const char *text, MazenCommand *command) {
  size_t i;
  for (i = 0; i < MAZEN_ARRAY_LEN(COMMANDS); ++i) {
    if (strcmp(COMMANDS[i].name, text) == 0) {
      *command = COMMANDS[i].command;
      return true;
    }
  }
  return false;
}

static bool take_option_value(int argc, char **argv, int *index,
                              const char *arg, const char *option_name,
                              const char **value, Diagnostic *diag) {
  const char *equals = strchr(arg, '=');
  if (equals != NULL) {
    *value = equals + 1;
    return true;
  }

  if (*index + 1 >= argc) {
    diag_set(diag, "%s requires a value", option_name);
    return false;
  }

  *value = argv[++(*index)];
  return true;
}

void cli_options_init(CliOptions *options) {
  options->command = MAZEN_CMD_BUILD;
  options->verbose = false;
  options->jobs = 0;
  options->jobs_set = false;
  options->compiler = NULL;
  options->c_standard = NULL;
  options->name_override = NULL;
  options->target_name = NULL;
  string_list_init(&options->include_dirs);
  string_list_init(&options->libs);
  string_list_init(&options->src_dirs);
  string_list_init(&options->excludes);
  string_list_init(&options->cflags);
  string_list_init(&options->ldflags);
  string_list_init(&options->run_args);
}

void cli_options_free(CliOptions *options) {
  free(options->compiler);
  free(options->c_standard);
  free(options->name_override);
  free(options->target_name);
  string_list_free(&options->include_dirs);
  string_list_free(&options->libs);
  string_list_free(&options->src_dirs);
  string_list_free(&options->excludes);
  string_list_free(&options->cflags);
  string_list_free(&options->ldflags);
  string_list_free(&options->run_args);
  cli_options_init(options);
}

bool cli_parse(int argc, char **argv, CliOptions *options, Diagnostic *diag) {
  bool seen_command = false;
  int i;

  for (i = 1; i < argc; ++i) {
    const char *arg = argv[i];

    if (strcmp(arg, "--") == 0) {
      if (options->command != MAZEN_CMD_RUN) {
        diag_set(diag, "`--` is only valid with `mazen run`");
        return false;
      }
      for (++i; i < argc; ++i) {
        string_list_push(&options->run_args, argv[i]);
      }
      break;
    }

    if (arg[0] != '-') {
      MazenCommand command;
      if (!seen_command && command_from_text(arg, &command)) {
        options->command = command;
        seen_command = true;
        continue;
      }
      diag_set(diag, "unexpected argument `%s`", arg);
      return false;
    }

    if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
      options->command = MAZEN_CMD_HELP;
      continue;
    }
    if (strcmp(arg, "--version") == 0) {
      options->command = MAZEN_CMD_VERSION;
      continue;
    }
    if (strcmp(arg, "--target") == 0 || string_starts_with(arg, "--target=")) {
      const char *value = NULL;
      if (!take_option_value(argc, argv, &i, arg, "--target", &value, diag)) {
        return false;
      }
      if (value[0] == '\0') {
        diag_set(diag, "--target requires a non-empty target name");
        return false;
      }
      free(options->target_name);
      options->target_name = mazen_xstrdup(value);
      continue;
    }
    if (strcmp(arg, "-j") == 0 || strcmp(arg, "--jobs") == 0 ||
        string_starts_with(arg, "--jobs=")) {
      const char *value = NULL;
      int jobs = 0;
      if (strcmp(arg, "-j") == 0) {
        if (i + 1 >= argc) {
          diag_set(diag, "-j requires a value");
          return false;
        }
        value = argv[++i];
      } else if (!take_option_value(argc, argv, &i, arg, "--jobs", &value,
                                    diag)) {
        return false;
      }
      if (!parse_job_count_value(
              value, strcmp(arg, "-j") == 0 ? "-j" : "--jobs", &jobs, diag)) {
        return false;
      }
      options->jobs = jobs;
      options->jobs_set = true;
      continue;
    }
    if (string_starts_with(arg, "-j") && strlen(arg) > 2) {
      int jobs = 0;
      if (!parse_job_count_value(arg + 2, "-j", &jobs, diag)) {
        return false;
      }
      options->jobs = jobs;
      options->jobs_set = true;
      continue;
    }
    if (strcmp(arg, "--std") == 0 || string_starts_with(arg, "--std=")) {
      const char *value = NULL;
      if (!take_option_value(argc, argv, &i, arg, "--std", &value, diag)) {
        return false;
      }
      if (!validate_standard_value(value, "--std", diag)) {
        return false;
      }
      free(options->c_standard);
      options->c_standard = mazen_xstrdup(value);
      continue;
    }
    if (strcmp(arg, "--verbose") == 0) {
      options->verbose = true;
      continue;
    }
    if (strcmp(arg, "--compiler") == 0 ||
        string_starts_with(arg, "--compiler=")) {
      const char *value = NULL;
      if (!take_option_value(argc, argv, &i, arg, "--compiler", &value, diag)) {
        return false;
      }
      free(options->compiler);
      options->compiler = mazen_xstrdup(value);
      continue;
    }
    if (strcmp(arg, "--name") == 0 || string_starts_with(arg, "--name=")) {
      const char *value = NULL;
      if (!take_option_value(argc, argv, &i, arg, "--name", &value, diag)) {
        return false;
      }
      free(options->name_override);
      options->name_override = mazen_xstrdup(value);
      continue;
    }
    if (strcmp(arg, "--lib") == 0 || string_starts_with(arg, "--lib=")) {
      const char *value = NULL;
      if (!take_option_value(argc, argv, &i, arg, "--lib", &value, diag)) {
        return false;
      }
      string_list_push(&options->libs, value);
      continue;
    }
    if (strcmp(arg, "-I") == 0 || strcmp(arg, "--include-dir") == 0 ||
        string_starts_with(arg, "--include-dir=")) {
      const char *value = NULL;
      if (strcmp(arg, "-I") == 0) {
        if (i + 1 >= argc) {
          diag_set(diag, "-I requires a value");
          return false;
        }
        value = argv[++i];
      } else if (!take_option_value(argc, argv, &i, arg, "--include-dir",
                                    &value, diag)) {
        return false;
      }
      string_list_push(&options->include_dirs, value);
      continue;
    }
    if (string_starts_with(arg, "-I") && strlen(arg) > 2) {
      string_list_push(&options->include_dirs, arg + 2);
      continue;
    }
    if (strcmp(arg, "--src-dir") == 0 ||
        string_starts_with(arg, "--src-dir=")) {
      const char *value = NULL;
      if (!take_option_value(argc, argv, &i, arg, "--src-dir", &value, diag)) {
        return false;
      }
      string_list_push(&options->src_dirs, value);
      continue;
    }
    if (strcmp(arg, "--exclude") == 0 ||
        string_starts_with(arg, "--exclude=")) {
      const char *value = NULL;
      if (!take_option_value(argc, argv, &i, arg, "--exclude", &value, diag)) {
        return false;
      }
      string_list_push(&options->excludes, value);
      continue;
    }
    if (strcmp(arg, "--cflag") == 0 || string_starts_with(arg, "--cflag=")) {
      const char *value = NULL;
      if (!take_option_value(argc, argv, &i, arg, "--cflag", &value, diag)) {
        return false;
      }
      string_list_push(&options->cflags, value);
      continue;
    }
    if (strcmp(arg, "--ldflag") == 0 || string_starts_with(arg, "--ldflag=")) {
      const char *value = NULL;
      if (!take_option_value(argc, argv, &i, arg, "--ldflag", &value, diag)) {
        return false;
      }
      string_list_push(&options->ldflags, value);
      continue;
    }

    diag_set(diag, "unknown option `%s`", arg);
    return false;
  }

  return true;
}

void cli_print_help(void) {
  puts("MAZEN " MAZEN_VERSION_STRING);
  puts("");
  puts("Usage:");
  puts("  mazen [command] [options]");
  puts("");
  puts("Commands:");
  puts("  build      Build the project in debug mode (default)");
  puts("  run        Build and run the detected executable");
  puts("  release    Build the project in release mode");
  puts("  clean      Remove build artifacts");
  puts("  doctor     Analyze project discovery and build inference");
  puts("  list       List detected entry targets");
  puts("  standards  List supported C language standards");
  puts("  help       Show this help");
  puts("");
  puts("Options:");
  puts("  --compiler PATH      Use a specific Clang binary");
  puts("  --target NAME        Build an explicit target from mazen.toml");
  puts("  -j, --jobs COUNT     Compile sources in parallel");
  puts("  --std VALUE          Select the C language standard");
  puts("  --name NAME          Override output binary name");
  puts("  --lib LIB            Link an extra library, repeatable");
  puts("  -I DIR               Add an include directory, repeatable");
  puts("  --include-dir DIR    Add an include directory, repeatable");
  puts("  --src-dir DIR        Add a preferred source root");
  puts("  --exclude PATH       Ignore an additional directory or subtree");
  puts("  --cflag FLAG         Add an extra compiler flag");
  puts("  --ldflag FLAG        Add an extra linker flag");
  puts("  --verbose            Print full compiler and linker commands");
  puts("  --version            Print version information");
  puts("");
  puts("Run arguments:");
  puts("  mazen run -- arg1 arg2");
}

const char *cli_command_name(MazenCommand command) {
  switch (command) {
  case MAZEN_CMD_BUILD:
    return "build";
  case MAZEN_CMD_RUN:
    return "run";
  case MAZEN_CMD_RELEASE:
    return "release";
  case MAZEN_CMD_CLEAN:
    return "clean";
  case MAZEN_CMD_DOCTOR:
    return "doctor";
  case MAZEN_CMD_LIST:
    return "list";
  case MAZEN_CMD_STANDARDS:
    return "standards";
  case MAZEN_CMD_HELP:
    return "help";
  case MAZEN_CMD_VERSION:
    return "version";
  default:
    return "unknown";
  }
}
