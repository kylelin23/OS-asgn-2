#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <sys/types.h>

typedef struct {
    int id;
    int priority;
    char **args;
    int nargs;
    pid_t pid;
    int done;
} Process;

#endif