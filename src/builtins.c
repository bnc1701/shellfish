#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "builtins.h"
#include "history.h"
#include "shell.h"

int should_exit = 0;
int final_exit_code = 0;

// keeps the previous directory around so "cd -" works
static char previous_dir[PATH_MAX] = "";

static int builtin_cd(command_t *cmd) {
    char current[PATH_MAX];
    if (getcwd(current, sizeof(current)) == NULL) {
        current[0] = '\0';
    }

    const char *target;

    if (cmd->arg_count < 2) {
        // no argument goes to home, standard behavior of every shell out there
        target = getenv("HOME");
        if (target == NULL) {
            fprintf(stderr, SHELL_NAME ": cd: couldnt find the HOME variable\n");
            return 1;
        }
    } else if (strcmp(cmd->args[1], "-") == 0) {
        if (strlen(previous_dir) == 0) {
            fprintf(stderr, SHELL_NAME ": cd: theres no previous directory yet\n");
            return 1;
        }
        target = previous_dir;
        printf("%s\n", target);
    } else {
        target = cmd->args[1];
    }

    if (chdir(target) != 0) {
        fprintf(stderr, SHELL_NAME ": cd: %s: doesnt exist or no permission\n", target);
        return 1;
    }

    strncpy(previous_dir, current, sizeof(previous_dir) - 1);
    previous_dir[sizeof(previous_dir) - 1] = '\0';
    return 0;
}

static int builtin_pwd(command_t *cmd) {
    (void) cmd; // doesnt use any argument, just here to shut up the unused warning
    char current[PATH_MAX];
    if (getcwd(current, sizeof(current)) != NULL) {
        printf("%s\n", current);
        return 0;
    }
    perror(SHELL_NAME ": pwd");
    return 1;
}

static int builtin_exit(command_t *cmd) {
    int code = 0;
    if (cmd->arg_count >= 2) {
        code = atoi(cmd->args[1]);
    }
    printf("later, take care\n");
    should_exit = 1;
    final_exit_code = code;
    return code;
}

static int builtin_export(command_t *cmd) {
    if (cmd->arg_count < 2) {
        fprintf(stderr, SHELL_NAME ": export: use it like this -> export NAME=value\n");
        return 1;
    }

    for (int i = 1; i < cmd->arg_count; i++) {
        char *equals = strchr(cmd->args[i], '=');
        if (equals == NULL) {
            fprintf(stderr, SHELL_NAME ": export: '%s' is missing the equals sign\n", cmd->args[i]);
            continue;
        }
        // temporarily splits NAME=value in two so we can use setenv
        *equals = '\0';
        setenv(cmd->args[i], equals + 1, 1);
        *equals = '='; // put the = back where it was, cant leave argv messed up
    }
    return 0;
}

static int builtin_unset(command_t *cmd) {
    if (cmd->arg_count < 2) {
        fprintf(stderr, SHELL_NAME ": unset: need to say the variable name\n");
        return 1;
    }
    for (int i = 1; i < cmd->arg_count; i++) {
        unsetenv(cmd->args[i]);
    }
    return 0;
}

static int builtin_history(command_t *cmd) {
    (void) cmd;
    history_show();
    return 0;
}

static int builtin_help(command_t *cmd) {
    (void) cmd;
    printf(SHELL_NAME " - a shell built from scratch in c just to learn stuff and use it for real\n\n");
    printf("builtin commands available:\n");
    printf("  cd [dir]         change directory (cd - goes back to the previous one)\n");
    printf("  pwd              print the current directory\n");
    printf("  export VAR=val   set an environment variable\n");
    printf("  unset VAR        remove an environment variable\n");
    printf("  history          show the commands typed so far\n");
    printf("  help             show this message right here\n");
    printf("  exit [code]      quit the shell\n\n");
    printf("anything else it tries to run as a normal system program\n");
    printf("you can use | for pipes, > >> < for redirection and & to run in the background\n");
    return 0;
}

int is_builtin(const char *name) {
    return strcmp(name, "cd") == 0 ||
           strcmp(name, "pwd") == 0 ||
           strcmp(name, "exit") == 0 ||
           strcmp(name, "export") == 0 ||
           strcmp(name, "unset") == 0 ||
           strcmp(name, "history") == 0 ||
           strcmp(name, "help") == 0;
}

int run_builtin(command_t *cmd) {
    const char *name = cmd->args[0];

    if (strcmp(name, "cd") == 0) return builtin_cd(cmd);
    if (strcmp(name, "pwd") == 0) return builtin_pwd(cmd);
    if (strcmp(name, "exit") == 0) return builtin_exit(cmd);
    if (strcmp(name, "export") == 0) return builtin_export(cmd);
    if (strcmp(name, "unset") == 0) return builtin_unset(cmd);
    if (strcmp(name, "history") == 0) return builtin_history(cmd);
    if (strcmp(name, "help") == 0) return builtin_help(cmd);

    // should never get here if is_builtin was checked before
    return 1;
}
