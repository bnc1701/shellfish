#ifndef PARSER_H
#define PARSER_H

#include "shell.h"

// represents a single command, like "ls -la"
// if it has redirection (< > >>) it stays saved here too
typedef struct {
    char *args[MAX_TOKENS];
    int arg_count;
    char *input_file;    // != NULL if theres a < file
    char *output_file;   // != NULL if theres a > or >> file
    int append_mode;     // 1 if its >>, 0 if its >
} command_t;

// one line can have several commands linked by pipe
// like "cat file | grep hi | wc -l"
typedef struct {
    command_t commands[MAX_PIPE];
    int command_count;
    int background;   // 1 if the line ends with &
} pipeline_t;

// takes the raw typed line and turns it into a pipeline_t struct
// returns 0 if it worked, -1 if the line was invalid (empty, syntax error etc)
int parse_line(char *line, pipeline_t *pipe_out);

// frees the memory allocated inside a pipeline_t (args, filenames, all that)
void free_pipeline(pipeline_t *p);

#endif
