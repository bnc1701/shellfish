#ifndef SHELL_H
#define SHELL_H

// max size of a line the user types in
#define LINE_SIZE 1024

// max number of tokens (arguments) per command
#define MAX_TOKENS 128

// max number of commands in a pipeline (like a | b | c)
#define MAX_PIPE 16

// name that shows up in the prompt, change it here if u want
#define SHELL_NAME "shellfish"

// main loop of the shell, keeps reading commands and running them
void shell_loop(void);

#endif
