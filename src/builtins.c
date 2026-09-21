#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"
#include "history.h"
#include "shell.h"

int should_exit;
int final_exit_code;

static char previous_dir[PATH_MAX];

static int parse_exit_code(const char *text, int *code) {
    char *end;
    long value = strtol(text, &end, 10);
    if (*text == '\0' || *end != '\0' || value < 0 || value > 255) return -1;
    *code = (int) value;
    return 0;
}

static int valid_name(const char *name) {
    if (name[0] == '\0' || (!((name[0] >= 'a' && name[0] <= 'z') || (name[0] >= 'A' && name[0] <= 'Z') || name[0] == '_'))) return 0;
    for (int i = 1; name[i] != '\0'; i++) {
        if (!((name[i] >= 'a' && name[i] <= 'z') || (name[i] >= 'A' && name[i] <= 'Z') || (name[i] >= '0' && name[i] <= '9') || name[i] == '_')) return 0;
    }
    return 1;
}

static int builtin_cd(command_t *command) {
    char current[PATH_MAX];
    if (getcwd(current, sizeof(current)) == NULL) current[0] = '\0';

    const char *target = command->arg_count < 2 ? getenv("HOME") : command->args[1];
    if (target == NULL) {
        fprintf(stderr, SHELL_NAME ": cd: home is not set\n");
        return 1;
    }
    if (strcmp(target, "-") == 0) {
        if (previous_dir[0] == '\0') {
            fprintf(stderr, SHELL_NAME ": cd: no previous directory\n");
            return 1;
        }
        target = previous_dir;
        printf("%s\n", target);
    }
    if (chdir(target) != 0) {
        fprintf(stderr, SHELL_NAME ": cd: %s: %s\n", target, strerror(errno));
        return 1;
    }
    strncpy(previous_dir, current, sizeof(previous_dir) - 1);
    previous_dir[sizeof(previous_dir) - 1] = '\0';
    return 0;
}

static int builtin_pwd(command_t *command) {
    (void) command;
    char current[PATH_MAX];
    if (getcwd(current, sizeof(current)) == NULL) {
        perror(SHELL_NAME ": pwd");
        return 1;
    }
    printf("%s\n", current);
    return 0;
}

static int builtin_exit(command_t *command) {
    int code = 0;
    if (command->arg_count > 2 || (command->arg_count == 2 && parse_exit_code(command->args[1], &code) != 0)) {
        fprintf(stderr, SHELL_NAME ": exit: expected a code from 0 to 255\n");
        return 2;
    }
    should_exit = 1;
    final_exit_code = code;
    return code;
}

static int builtin_export(command_t *command) {
    int result = 0;
    for (int i = 1; i < command->arg_count; i++) {
        char *equals = strchr(command->args[i], '=');
        if (equals == NULL) {
            fprintf(stderr, SHELL_NAME ": export: expected name=value\n");
            result = 1;
            continue;
        }
        *equals = '\0';
        if (!valid_name(command->args[i]) || setenv(command->args[i], equals + 1, 1) != 0) {
            fprintf(stderr, SHELL_NAME ": export: invalid name\n");
            result = 1;
        }
        *equals = '=';
    }
    return result;
}

static int builtin_unset(command_t *command) {
    int result = 0;
    for (int i = 1; i < command->arg_count; i++) {
        if (!valid_name(command->args[i]) || unsetenv(command->args[i]) != 0) result = 1;
    }
    return result;
}

static int builtin_history(command_t *command) {
    (void) command;
    history_show();
    return 0;
}

static int builtin_help(command_t *command) {
    (void) command;
    printf("builtins: cd pwd export unset history help exit\n");
    printf("operators: | < > >> &\n");
    return 0;
}

int is_builtin(const char *name) {
    return name != NULL && (strcmp(name, "cd") == 0 || strcmp(name, "pwd") == 0 || strcmp(name, "exit") == 0 || strcmp(name, "export") == 0 || strcmp(name, "unset") == 0 || strcmp(name, "history") == 0 || strcmp(name, "help") == 0);
}

int run_builtin(command_t *command) {
    const char *name = command->args[0];
    if (strcmp(name, "cd") == 0) return builtin_cd(command);
    if (strcmp(name, "pwd") == 0) return builtin_pwd(command);
    if (strcmp(name, "exit") == 0) return builtin_exit(command);
    if (strcmp(name, "export") == 0) return builtin_export(command);
    if (strcmp(name, "unset") == 0) return builtin_unset(command);
    if (strcmp(name, "history") == 0) return builtin_history(command);
    if (strcmp(name, "help") == 0) return builtin_help(command);
    return 1;
}
