#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "builtins.h"
#include "exec.h"
#include "shell.h"

#define max_bg_jobs 64

static pid_t bg_jobs[max_bg_jobs];
static int bg_jobs_count;
static pid_t shell_pgid;
static int shell_terminal = -1;
static int job_control_ready;

static void add_bg_job(pid_t pid) {
    if (bg_jobs_count == max_bg_jobs) {
        int status;
        waitpid(bg_jobs[0], &status, 0);
        for (int i = 1; i < bg_jobs_count; i++) {
            bg_jobs[i - 1] = bg_jobs[i];
        }
        bg_jobs_count--;
    }
    bg_jobs[bg_jobs_count++] = pid;
}

static void remove_bg_job(int index) {
    for (int i = index + 1; i < bg_jobs_count; i++) {
        bg_jobs[i - 1] = bg_jobs[i];
    }
    bg_jobs_count--;
}

static void setup_job_control(void) {
    shell_terminal = STDIN_FILENO;
    if (!isatty(shell_terminal)) {
        return;
    }

    shell_pgid = getpid();
    if (setpgid(shell_pgid, shell_pgid) < 0 && errno != EACCES) {
        return;
    }
    if (tcsetpgrp(shell_terminal, shell_pgid) < 0) {
        return;
    }

    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    job_control_ready = 1;
}

static void give_terminal(pid_t pgid) {
    if (job_control_ready) {
        tcsetpgrp(shell_terminal, pgid);
    }
}

static void apply_redirections(command_t *command) {
    if (command->input_file != NULL) {
        int fd = open(command->input_file, O_RDONLY);
        if (fd < 0 || dup2(fd, STDIN_FILENO) < 0) {
            fprintf(stderr, SHELL_NAME ": %s: cannot open input\n", command->input_file);
            if (fd >= 0) close(fd);
            _exit(1);
        }
        close(fd);
    }

    if (command->output_file != NULL) {
        int flags = O_WRONLY | O_CREAT;
        flags |= command->append_mode ? O_APPEND : O_TRUNC;
        int fd = open(command->output_file, flags, 0644);
        if (fd < 0 || dup2(fd, STDOUT_FILENO) < 0) {
            fprintf(stderr, SHELL_NAME ": %s: cannot open output\n", command->output_file);
            if (fd >= 0) close(fd);
            _exit(1);
        }
        close(fd);
    }
}

static void run_external_command(command_t *command) {
    execvp(command->args[0], command->args);
    fprintf(stderr, SHELL_NAME ": %s: command not found\n", command->args[0]);
    _exit(errno == ENOENT ? 127 : 126);
}

static void reset_child_signals(void) {
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
}

void check_finished_jobs(void) {
    int i = 0;
    while (i < bg_jobs_count) {
        int status;
        pid_t result = waitpid(bg_jobs[i], &status, WNOHANG);
        if (result == 0) {
            i++;
            continue;
        }
        if (result < 0 && errno == EINTR) {
            continue;
        }
        printf("[bg %d] done\n", bg_jobs[i]);
        remove_bg_job(i);
    }
}

void wait_for_jobs(void) {
    while (bg_jobs_count > 0) {
        int status;
        if (waitpid(bg_jobs[0], &status, 0) < 0 && errno == EINTR) {
            continue;
        }
        remove_bg_job(0);
    }
}

void execute_pipeline(pipeline_t *pipeline) {
    int count = pipeline->command_count;
    if (count == 1 && !pipeline->background && is_builtin(pipeline->commands[0].args[0])) {
        run_builtin(&pipeline->commands[0]);
        return;
    }

    int input_fd = STDIN_FILENO;
    pid_t pids[MAX_PIPE];
    pid_t process_group = 0;
    int pid_count = 0;

    for (int i = 0; i < count; i++) {
        int pipe_fd[2] = {-1, -1};
        int has_next = i < count - 1;
        if (has_next && pipe(pipe_fd) < 0) {
            perror(SHELL_NAME ": pipe");
            break;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror(SHELL_NAME ": fork");
            if (pipe_fd[0] >= 0) close(pipe_fd[0]);
            if (pipe_fd[1] >= 0) close(pipe_fd[1]);
            break;
        }

        if (pid == 0) {
            reset_child_signals();
            if (process_group == 0) process_group = getpid();
            setpgid(0, process_group);

            if (input_fd != STDIN_FILENO) {
                if (dup2(input_fd, STDIN_FILENO) < 0) _exit(1);
                close(input_fd);
            }
            if (has_next) {
                close(pipe_fd[0]);
                if (dup2(pipe_fd[1], STDOUT_FILENO) < 0) _exit(1);
                close(pipe_fd[1]);
            }

            apply_redirections(&pipeline->commands[i]);
            if (is_builtin(pipeline->commands[i].args[0])) {
                _exit(run_builtin(&pipeline->commands[i]));
            }
            run_external_command(&pipeline->commands[i]);
        }

        if (process_group == 0) process_group = pid;
        setpgid(pid, process_group);
        pids[pid_count++] = pid;

        if (input_fd != STDIN_FILENO) close(input_fd);
        if (has_next) {
            close(pipe_fd[1]);
            input_fd = pipe_fd[0];
        }
    }

    if (pid_count == 0) return;

    if (pipeline->background) {
        printf("[bg] %d\n", process_group);
        for (int i = 0; i < pid_count; i++) add_bg_job(pids[i]);
        return;
    }

    give_terminal(process_group);
    for (int i = 0; i < pid_count; i++) {
        int status;
        while (waitpid(pids[i], &status, 0) < 0 && errno == EINTR) {
        }
    }
    give_terminal(shell_pgid);
}
