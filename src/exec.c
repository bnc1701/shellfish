#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

#include "exec.h"
#include "builtins.h"
#include "shell.h"

// simple list of background jobs that havent been reaped yet
// (reaped = called waitpid on them so they dont stay zombies forever)
#define MAX_BG_JOBS 64
static pid_t bg_jobs[MAX_BG_JOBS];
static int bg_jobs_count = 0;

static void add_bg_job(pid_t pid) {
    if (bg_jobs_count >= MAX_BG_JOBS) {
        // list is full, do a blocking waitpid on the oldest one to make room
        int status;
        waitpid(bg_jobs[0], &status, 0);
        for (int i = 1; i < bg_jobs_count; i++) bg_jobs[i - 1] = bg_jobs[i];
        bg_jobs_count--;
    }
    bg_jobs[bg_jobs_count++] = pid;
}

void check_finished_jobs(void) {
    int i = 0;
    while (i < bg_jobs_count) {
        int status;
        pid_t result = waitpid(bg_jobs[i], &status, WNOHANG);

        if (result == 0) {
            // still running, leave it alone
            i++;
            continue;
        }

        // it finished (or errored out, like already being reaped before) so drop it
        printf("[bg %d] done\n", bg_jobs[i]);
        for (int j = i + 1; j < bg_jobs_count; j++) bg_jobs[j - 1] = bg_jobs[j];
        bg_jobs_count--;
    }
}

// applies the < > >> redirections of the command in the current process
// only call this after fork, inside the child process
static void apply_redirections(command_t *c) {
    if (c->input_file != NULL) {
        int fd = open(c->input_file, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, SHELL_NAME ": %s: file doesnt exist\n", c->input_file);
            exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (c->output_file != NULL) {
        int flags = O_WRONLY | O_CREAT | (c->append_mode ? O_APPEND : O_TRUNC);
        int fd = open(c->output_file, flags, 0644);
        if (fd < 0) {
            fprintf(stderr, SHELL_NAME ": %s: couldnt open it for writing\n", c->output_file);
            exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}

// runs a single external command (not a builtin) through execvp
// this replaces the whole process, so it only returns if something failed
static void run_external_command(command_t *c) {
    execvp(c->args[0], c->args);

    // if we got here execvp failed
    fprintf(stderr, SHELL_NAME ": %s: command not found\n", c->args[0]);
    exit(127);
}

void execute_pipeline(pipeline_t *p) {
    int count = p->command_count;

    // most common and fastest case: a single command, no pipe, no background
    // if its a builtin run it directly in the shell process itself (so cd
    // actually works, for example)
    if (count == 1 && !p->background && is_builtin(p->commands[0].args[0])) {
        run_builtin(&p->commands[0]);
        return;
    }

    int input_fd = STDIN_FILENO; // where the next command in the pipeline reads from
    pid_t pids[MAX_PIPE];
    int pid_count = 0;

    for (int i = 0; i < count; i++) {
        int pipe_fd[2];
        int has_next = (i < count - 1);

        if (has_next) {
            if (pipe(pipe_fd) < 0) {
                perror(SHELL_NAME ": pipe");
                return;
            }
        }

        pid_t pid = fork();

        if (pid < 0) {
            perror(SHELL_NAME ": fork");
            return;
        }

        if (pid == 0) {
            // child process

            // ctrl+c should kill the child normally, not the shell
            signal(SIGINT, SIG_DFL);

            // connects the input to the previous pipe, if this isnt the first command
            if (input_fd != STDIN_FILENO) {
                dup2(input_fd, STDIN_FILENO);
                close(input_fd);
            }

            // connects the output to the next pipe, if this isnt the last command
            if (has_next) {
                close(pipe_fd[0]);
                dup2(pipe_fd[1], STDOUT_FILENO);
                close(pipe_fd[1]);
            }

            // manual redirection (< > >>) always takes priority over the pipe
            apply_redirections(&p->commands[i]);

            if (is_builtin(p->commands[i].args[0])) {
                int code = run_builtin(&p->commands[i]);
                exit(code);
            }

            run_external_command(&p->commands[i]);
        }

        // parent process continues here
        pids[pid_count++] = pid;

        if (input_fd != STDIN_FILENO) {
            close(input_fd);
        }

        if (has_next) {
            close(pipe_fd[1]);
            input_fd = pipe_fd[0];
        }
    }

    if (p->background) {
        // dont wait right now, but keep track of everyone in the pipeline to
        // reap later (if we dont track the middle processes too they turn
        // into zombies once they finish)
        printf("[bg] %d\n", pids[pid_count - 1]);
        for (int i = 0; i < pid_count; i++) {
            add_bg_job(pids[i]);
        }
        return;
    }

    // wait for the whole pipeline to finish before handing the prompt back
    for (int i = 0; i < pid_count; i++) {
        int status;
        waitpid(pids[i], &status, 0);
    }
}
