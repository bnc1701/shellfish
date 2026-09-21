#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "builtins.h"
#include "exec.h"
#include "history.h"
#include "parser.h"
#include "shell.h"

static void strip_newline(char *line) {
    size_t length = strlen(line);
    if (length > 0 && line[length - 1] == '\n') {
        line[length - 1] = '\0';
    }
}

static int is_blank_line(const char *line) {
    for (int i = 0; line[i] != '\0'; i++) {
        if (line[i] != ' ' && line[i] != '\t') {
            return 0;
        }
    }
    return 1;
}

void shell_loop(void) {
    char line[LINE_SIZE];
    pipeline_t pipeline;

    signal(SIGINT, SIG_IGN);
    setup_job_control();
    history_load();

    while (!should_exit) {
        check_finished_jobs();
        fputs("shellfish$ ", stdout);
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            fputc('\n', stdout);
            break;
        }

        strip_newline(line);
        if (is_blank_line(line)) {
            continue;
        }

        history_add(line);
        char copy[LINE_SIZE];
        strncpy(copy, line, sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';

        if (parse_line(copy, &pipeline) != 0) {
            continue;
        }

        execute_pipeline(&pipeline);
        free_pipeline(&pipeline);
    }

    wait_for_jobs();
    history_save();
    history_free();
}
