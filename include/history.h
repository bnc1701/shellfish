#ifndef HISTORY_H
#define HISTORY_H

// how many commands to keep in the history
#define MAX_HISTORY 500

// loads the saved history from the file (if it exists) into memory
void history_load(void);

// adds a new line to the history, skips it if its the same as the last one
void history_add(const char *line);

// prints the whole history to the screen, numbered
void history_show(void);

// saves the current history to the file, called when the shell closes
void history_save(void);

// frees the memory used by the history
void history_free(void);

#endif
