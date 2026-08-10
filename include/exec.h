#ifndef EXEC_H
#define EXEC_H

#include "parser.h"

// takes the already parsed pipeline and gets it running
// handles fork, pipe between commands, redirection and the & background thing
void execute_pipeline(pipeline_t *p);

// checks up on background jobs that already finished and cleans them up
// (otherwise it turns into a zombie process which is nasty). call this
// before every prompt
void check_finished_jobs(void);

#endif
