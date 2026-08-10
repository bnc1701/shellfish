#ifndef BUILTINS_H
#define BUILTINS_H

#include "parser.h"

// becomes 1 when the user runs the exit command, the main loop watches this
extern int should_exit;

// exit code that exit received, like "exit 7", main returns this to the os
extern int final_exit_code;

// checks if the command name is a builtin (cd, exit, pwd, etc)
int is_builtin(const char *name);

// runs the builtin and returns its exit code
// only call this after confirming with is_builtin that it can actually run
int run_builtin(command_t *cmd);

#endif
