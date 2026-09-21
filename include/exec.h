#ifndef EXEC_H
#define EXEC_H

#include "parser.h"

void execute_pipeline(pipeline_t *pipeline);
void check_finished_jobs(void);
void wait_for_jobs(void);

#endif
