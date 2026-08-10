#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <limits.h>
#include <pwd.h>

#include "shell.h"
#include "parser.h"
#include "exec.h"
#include "builtins.h"
#include "history.h"

// builds the prompt like "user:~/folder$ " swapping home for ~ when possible
static void build_prompt(char *destination, size_t size) {
    char cwd[PATH_MAX];
    const char *home = getenv("HOME");

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        strcpy(cwd, "?");
    }

    const char *shown_path = cwd;
    char shortened[PATH_MAX];

    if (home != NULL) {
        size_t home_len = strlen(home);
        if (strncmp(cwd, home, home_len) == 0) {
            snprintf(shortened, sizeof(shortened), "~%s", cwd + home_len);
            shown_path = shortened;
        }
    }

    struct passwd *user_info = getpwuid(getuid());
    const char *username = (user_info != NULL) ? user_info->pw_name : "user";

    snprintf(destination, size, "%s:%s$ ", username, shown_path);
}

// strips the \n that fgets leaves at the end of the line
static void strip_newline(char *line) {
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n') {
        line[len - 1] = '\0';
    }
}

// checks if the line is just whitespace (so we dont clutter the history
// with empty lines)
static int is_blank_line(const char *line) {
    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] != ' ' && line[i] != '\t') return 0;
    }
    return 1;
}

void shell_loop(void) {
    char line[LINE_SIZE];
    char prompt[PATH_MAX + 128];
    pipeline_t current_pipeline;

    // if ctrl+c happens at the prompt the shell keeps living, only the
    // command thats running dies
    signal(SIGINT, SIG_IGN);

    history_load();

    while (!should_exit) {
        check_finished_jobs();
        build_prompt(prompt, sizeof(prompt));
        printf("%s", prompt);
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            // ctrl+d, now this actually exits the shell
            printf("\n");
            break;
        }

        strip_newline(line);

        if (is_blank_line(line)) {
            continue;
        }

        history_add(line);

        // the parser messes with the string it gets so work on a copy
        char copy[LINE_SIZE];
        strncpy(copy, line, sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';

        if (parse_line(copy, &current_pipeline) != 0) {
            continue; // invalid line, the parser already printed the error if needed
        }

        execute_pipeline(&current_pipeline);
        free_pipeline(&current_pipeline);
    }

    history_save();
    history_free();
}
