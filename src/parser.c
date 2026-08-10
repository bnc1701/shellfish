#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "parser.h"

// reads the var name right after the $ (like $HOME or ${HOME}) and copies
// its value into destination. returns how many chars it wrote there
// *p ends up pointing to whatever comes after the var name
static int expand_variable(char **p, char *destination) {
    char name[256];
    int i = 0;
    int has_braces = 0;

    if (**p == '$') {
        // $$ is the shell's own pid, same deal as in bash
        (*p)++;
        int len = snprintf(destination, 32, "%d", getpid());
        return len;
    }

    if (**p == '{') {
        has_braces = 1;
        (*p)++;
    }

    while (isalnum((unsigned char) **p) || **p == '_') {
        if (i < (int) sizeof(name) - 1) name[i++] = **p;
        (*p)++;
    }
    name[i] = '\0';

    if (has_braces && **p == '}') (*p)++;

    if (i == 0) {
        // it was just a lone $ with no name after it, keep it as plain text
        destination[0] = '$';
        return 1;
    }

    const char *value = getenv(name);
    if (value == NULL) return 0;

    int len = strlen(value);
    memcpy(destination, value, len);
    return len;
}

// breaks the whole line into tokens, respecting single and double quotes
// each special symbol (| < >) becomes its own token even if its glued
// to the text, like "ls>output.txt" has to become ["ls", ">", "output.txt"]
static int tokenize(char *line, char *tokens[], int max_tokens) {
    int count = 0;
    char *p = line;

    while (*p != '\0') {
        // skip whitespace
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;

        if (count >= max_tokens - 1) {
            fprintf(stderr, SHELL_NAME ": command too long, chill\n");
            for (int k = 0; k < count; k++) free(tokens[k]);
            return -1;
        }

        // special symbols that are a token on their own
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
                p += 1;
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

        // regular token, might come wrapped in quotes
        char buffer[LINE_SIZE];
        int i = 0;

        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '|' &&
               *p != '>' && *p != '<' && *p != '&') {

            if (*p == '\'') {
                // single quote: take everything raw, no var expansion at all
                p++;
                while (*p != '\0' && *p != '\'') {
                    buffer[i++] = *p;
                    p++;
                }
                if (*p == '\'') p++;
            } else if (*p == '"') {
                // double quote: keep the text but still expand $variables inside
                p++;
                while (*p != '\0' && *p != '"') {
                    if (*p == '$') {
                        p++;
                        i += expand_variable(&p, buffer + i);
                    } else {
                        buffer[i++] = *p;
                        p++;
                    }
                }
                if (*p == '"') p++;
            } else if (*p == '$') {
                p++;
                i += expand_variable(&p, buffer + i);
            } else {
                buffer[i++] = *p;
                p++;
            }
        }
        buffer[i] = '\0';

        if (i > 0) {
            tokens[count++] = strdup(buffer);
        }
    }

    tokens[count] = NULL;
    return count;
}

// zeroes out a command struct so its all clean before filling it up
static void init_command(command_t *c) {
    c->arg_count = 0;
    c->input_file = NULL;
    c->output_file = NULL;
    c->append_mode = 0;
    for (int i = 0; i < MAX_TOKENS; i++) c->args[i] = NULL;
}

int parse_line(char *line, pipeline_t *pipe_out) {
    char *tokens[MAX_TOKENS * MAX_PIPE];

    pipe_out->command_count = 0;
    pipe_out->background = 0;

    int token_count = tokenize(line, tokens, MAX_TOKENS * MAX_PIPE);
    if (token_count <= 0) {
        return -1;
    }

    // if the last token is & mark it as background and get rid of it
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

    // index of where we stopped consuming tokens, used if something
    // breaks midway so we know what tokens are still unclaimed and
    // need to be freed
    int i = 0;

    for (i = 0; i < token_count; i++) {
        char *tok = tokens[i];

        if (strcmp(tok, "|") == 0) {
            free(tok);
            if (pipe_out->command_count >= MAX_PIPE) {
                fprintf(stderr, SHELL_NAME ": too many pipes stacked up, cut it down\n");
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
                fprintf(stderr, SHELL_NAME ": missing filename after the >\n");
                goto syntax_error;
            }
            current->output_file = tokens[++i];
            current->append_mode = append;
            continue;
        }

        if (strcmp(tok, "<") == 0) {
            free(tok);
            if (i + 1 >= token_count) {
                fprintf(stderr, SHELL_NAME ": missing filename after the <\n");
                goto syntax_error;
            }
            current->input_file = tokens[++i];
            continue;
        }

        // regular token, goes in as an argument of the current command
        if (current->arg_count < MAX_TOKENS - 1) {
            current->args[current->arg_count++] = tok;
        } else {
            free(tok);
        }
    }

    // close each command's argv with NULL, otherwise execvp wont know where it ends
    for (int k = 0; k < pipe_out->command_count; k++) {
        pipe_out->commands[k].args[pipe_out->commands[k].arg_count] = NULL;
    }

    // if any command in the pipeline ended up with zero args the syntax is broken
    // (like a lone "cat |", or "| ls", or just a "|" hanging out there)
    for (int k = 0; k < pipe_out->command_count; k++) {
        if (pipe_out->commands[k].arg_count == 0) {
            fprintf(stderr, SHELL_NAME ": syntax error near the pipe\n");
            goto syntax_error;
        }
    }

    return 0;

syntax_error:
    // free whatever tokens werent claimed by the pipeline yet
    for (int k = i + 1; k < token_count; k++) {
        free(tokens[k]);
    }
    // and free everything that was already built before the error happened
    free_pipeline(pipe_out);
    return -1;
}

void free_pipeline(pipeline_t *p) {
    for (int i = 0; i < p->command_count; i++) {
        command_t *c = &p->commands[i];
        for (int j = 0; j < c->arg_count; j++) {
            free(c->args[j]);
        }
        if (c->input_file) free(c->input_file);
        if (c->output_file) free(c->output_file);
    }
    p->command_count = 0;
}
