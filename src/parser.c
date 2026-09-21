#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "parser.h"
#include "token_scan.h"

// reads the var name right after the $ (like $HOME or ${HOME}) and copies
// its value into destination. returns how many chars it wrote there
static int expand_variable(char **p, char *destination) {
    char name[256];
    int i = 0;
    int has_braces = 0;

    if (**p == '$') {
        (*p)++;
        return snprintf(destination, 32, "%d", getpid());
    }

    if (**p == '{') {
        has_braces = 1;
        (*p)++;
    }

    while (isalnum((unsigned char) **p) || **p == '_') {
        if (i < (int) sizeof(name) - 1) {
            name[i++] = **p;
        }
        (*p)++;
    }
    name[i] = '\0';

    if (has_braces && **p == '}') {
        (*p)++;
    }

    if (i == 0) {
        destination[0] = '$';
        return 1;
    }

    const char *value = getenv(name);
    if (value == NULL) {
        return 0;
    }

    size_t len = strlen(value);
    if (len >= (size_t) LINE_SIZE) {
        len = (size_t) LINE_SIZE - 1;
    }
    memcpy(destination, value, len);
    return (int) len;
}

static int append_char(char *buffer, size_t *length, char ch) {
    if (*length >= (size_t) LINE_SIZE - 1) {
        return -1;
    }
    buffer[(*length)++] = ch;
    return 0;
}

static int tokenize(char *line, char *tokens[], int max_tokens) {
    int count = 0;
    char *p = line;

    while (*p != '\0') {
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (*p == '\0') {
            break;
        }

        if (count >= max_tokens - 1) {
            fprintf(stderr, SHELL_NAME ": command too long\n");
            for (int k = 0; k < count; k++) {
                free(tokens[k]);
            }
            return -1;
        }

        if (*p == '|') {
            tokens[count++] = strdup("|");
            p++;
            continue;
        }
        if (*p == '>') {
            if (*(p + 1) == '>') {
                tokens[count++] = strdup(">>");
                p += 2;
            } else {
                tokens[count++] = strdup(">");
                p++;
            }
            continue;
        }
        if (*p == '<') {
            tokens[count++] = strdup("<");
            p++;
            continue;
        }
        if (*p == '&') {
            tokens[count++] = strdup("&");
            p++;
            continue;
        }

        char buffer[LINE_SIZE];
        size_t length = 0;

        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '|' && *p != '>' && *p != '<' && *p != '&') {
            if (*p == '\'') {
                p++;
                while (*p != '\0' && *p != '\'') {
                    if (append_char(buffer, &length, *p) != 0) {
                        fprintf(stderr, SHELL_NAME ": token too long\n");
                        for (int k = 0; k < count; k++) {
                            free(tokens[k]);
                        }
                        return -1;
                    }
                    p++;
                }
                if (*p == '\'') {
                    p++;
                }
            } else if (*p == '"') {
                p++;
                while (*p != '\0' && *p != '"') {
                    if (*p == '$') {
                        p++;
                        int written = expand_variable(&p, buffer + length);
                        if (written < 0) {
                            written = 0;
                        }
                        length += (size_t) written;
                        if (length >= (size_t) LINE_SIZE - 1) {
                            fprintf(stderr, SHELL_NAME ": token too long\n");
                            for (int k = 0; k < count; k++) {
                                free(tokens[k]);
                            }
                            return -1;
                        }
                    } else {
                        if (append_char(buffer, &length, *p) != 0) {
                            fprintf(stderr, SHELL_NAME ": token too long\n");
                            for (int k = 0; k < count; k++) {
                                free(tokens[k]);
                            }
                            return -1;
                        }
                        p++;
                    }
                }
                if (*p == '"') {
                    p++;
                }
            } else if (*p == '$') {
                p++;
                int written = expand_variable(&p, buffer + length);
                if (written < 0) {
                    written = 0;
                }
                length += (size_t) written;
                if (length >= (size_t) LINE_SIZE - 1) {
                    fprintf(stderr, SHELL_NAME ": token too long\n");
                    for (int k = 0; k < count; k++) {
                        free(tokens[k]);
                    }
                    return -1;
                }
            } else {
                size_t step = scan_token_end(p, strlen(p));
                if (step > (size_t) LINE_SIZE - length - 1) {
                    step = (size_t) LINE_SIZE - length - 1;
                }
                if (step > 0) {
                    memcpy(buffer + length, p, step);
                    length += step;
                    p += step;
                }
                break;
            }
        }

        buffer[length] = '\0';
        if (length > 0) {
            tokens[count++] = strdup(buffer);
        }
    }

    tokens[count] = NULL;
    return count;
}

static void init_command(command_t *command) {
    command->arg_count = 0;
    command->input_file = NULL;
    command->output_file = NULL;
    command->append_mode = 0;
    for (int i = 0; i < MAX_TOKENS; i++) {
        command->args[i] = NULL;
    }
}

int parse_line(char *line, pipeline_t *pipe_out) {
    char *tokens[MAX_TOKENS * MAX_PIPE];
    int token_count;
    int i = 0;

    pipe_out->command_count = 0;
    pipe_out->background = 0;

    token_count = tokenize(line, tokens, MAX_TOKENS * MAX_PIPE);
    if (token_count <= 0) {
        return -1;
    }

    if (strcmp(tokens[token_count - 1], "&") == 0) {
        pipe_out->background = 1;
        free(tokens[token_count - 1]);
        token_count--;
    }

    if (token_count == 0) {
        return -1;
    }

    command_t *current = &pipe_out->commands[0];
    init_command(current);
    pipe_out->command_count = 1;

    for (i = 0; i < token_count; i++) {
        char *tok = tokens[i];

        if (strcmp(tok, "|") == 0) {
            free(tok);
            if (pipe_out->command_count >= MAX_PIPE) {
                fprintf(stderr, SHELL_NAME ": too many pipes\n");
                goto syntax_error;
            }
            current = &pipe_out->commands[pipe_out->command_count];
            init_command(current);
            pipe_out->command_count++;
            continue;
        }

        if (strcmp(tok, ">") == 0 || strcmp(tok, ">>") == 0) {
            int append = (strcmp(tok, ">>") == 0);
            free(tok);
            if (i + 1 >= token_count) {
                fprintf(stderr, SHELL_NAME ": missing filename after redirection\n");
                goto syntax_error;
            }
            current->output_file = tokens[++i];
            current->append_mode = append;
            continue;
        }

        if (strcmp(tok, "<") == 0) {
            free(tok);
            if (i + 1 >= token_count) {
                fprintf(stderr, SHELL_NAME ": missing filename after input redirection\n");
                goto syntax_error;
            }
            current->input_file = tokens[++i];
            continue;
        }

        if (current->arg_count < MAX_TOKENS - 1) {
            current->args[current->arg_count++] = tok;
        } else {
            free(tok);
        }
    }

    for (int k = 0; k < pipe_out->command_count; k++) {
        pipe_out->commands[k].args[pipe_out->commands[k].arg_count] = NULL;
    }

    for (int k = 0; k < pipe_out->command_count; k++) {
        if (pipe_out->commands[k].arg_count == 0) {
            fprintf(stderr, SHELL_NAME ": syntax error near pipe\n");
            goto syntax_error;
        }
    }

    return 0;

syntax_error:
    for (int k = i + 1; k < token_count; k++) {
        free(tokens[k]);
    }
    free_pipeline(pipe_out);
    return -1;
}

void free_pipeline(pipeline_t *p) {
    for (int i = 0; i < p->command_count; i++) {
        command_t *command = &p->commands[i];
        for (int j = 0; j < command->arg_count; j++) {
            free(command->args[j]);
        }
        if (command->input_file) {
            free(command->input_file);
        }
        if (command->output_file) {
            free(command->output_file);
        }
    }
    p->command_count = 0;
}
