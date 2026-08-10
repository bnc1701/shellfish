#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "history.h"

// keeps the commands typed this session (and the previous ones too)
static char *list[MAX_HISTORY];
static int total = 0;

// builds the path of the history file, like /home/someone/.shellfish_history
// returns a static pointer, dont need to free it
static const char *file_path(void) {
    static char path[512];
    const char *home = getenv("HOME");
    if (home == NULL) home = ".";
    snprintf(path, sizeof(path), "%s/.shellfish_history", home);
    return path;
}

void history_load(void) {
    FILE *f = fopen(file_path(), "r");
    if (f == NULL) {
        return; // no saved history yet, no biggie
    }

    char line[1024];
    while (fgets(line, sizeof(line), f) != NULL && total < MAX_HISTORY) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (strlen(line) == 0) continue;
        list[total++] = strdup(line);
    }

    fclose(f);
}

void history_add(const char *line) {
    if (line == NULL || strlen(line) == 0) return;

    // if its the same as the last command dont even add it, gets too repetitive
    if (total > 0 && strcmp(list[total - 1], line) == 0) return;

    if (total >= MAX_HISTORY) {
        // full already, drop the oldest one and shift everything over
        free(list[0]);
        for (int i = 1; i < total; i++) list[i - 1] = list[i];
        total--;
    }

    list[total++] = strdup(line);
}

void history_show(void) {
    for (int i = 0; i < total; i++) {
        printf("%4d  %s\n", i + 1, list[i]);
    }
}

void history_save(void) {
    FILE *f = fopen(file_path(), "w");
    if (f == NULL) return; // no permission or whatever, just skip it

    for (int i = 0; i < total; i++) {
        fprintf(f, "%s\n", list[i]);
    }

    fclose(f);
}

void history_free(void) {
    for (int i = 0; i < total; i++) {
        free(list[i]);
    }
    total = 0;
}
