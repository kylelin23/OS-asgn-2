#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "schedule.h"
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Please include two parameters: time quantum and file. ");
        return 1;
    }

    // Parse inputs
    long quantum = strtol(argv[1], NULL, 10);

    // Read file
    FILE *fp = fopen(argv[2], "r");

    if (fp == NULL) {
        perror(argv[2]);
        return 1;
    }

    char *line = NULL;
    size_t len = 0;
    int capacity = 4;
    int count = 0;
    Process *procs = malloc(capacity * sizeof(Process));

    // For each process in input:
    while (getline(&line, &len, fp) != -1) {
        char **fields = NULL;
        int nfields = 0;

        // Cut by tabs
        char *tok = strtok(line, "\t\r\n");
        while (tok != NULL) {
            fields = realloc(fields, (nfields + 1) * sizeof(char *));
            fields[nfields] = tok;
            nfields++;
            tok = strtok(NULL, "\t\r\n");
        }

        // If empty line
        if (nfields < 3) {
            free(fields);
            continue;
        }

        // Allocate more memory if we run out of space
        if (count == capacity) {
            capacity *= 2;
            procs = realloc(procs, capacity * sizeof(Process));
        }

        // Fill in process struct
        Process *p = &procs[count];
        p->id = atoi(fields[0]);
        p->priority = atoi(fields[1]);
        p->pid = 0;
        p->done = 0;
        p->nargs = nfields - 2;
        p->args = malloc((p->nargs + 1) * sizeof(char *));
        char *prog = fields[2];
        if (strchr(prog, '/') == NULL) {
            p->args[0] = malloc(strlen(prog) + 3);
            sprintf(p->args[0], "./%s", prog);
        } else {
            p->args[0] = strdup(prog);
        }

        // Remove surrounding quotes
        for (int i = 1; i < p->nargs; i++) {
            char *s = fields[i + 2];
            size_t n = strlen(s);
            if (n >= 2 && s[0] == '"' && s[n - 1] == '"') {
                p->args[i] = strndup(s + 1, n - 2);
            } else {
                p->args[i] = strdup(s);
            }
        }
        p->args[p->nargs] = NULL;

        count++;
        free(fields);
    }

    // Free memory and close files
    free(line);
    fclose(fp);

    // Sort processes by priority
    for (int i = 1; i < count; i++) {
        Process current = procs[i];
        int j = i - 1;
        while (j >= 0 && procs[j].priority > current.priority) {
            procs[j + 1] = procs[j];
            j--;
        }
        procs[j + 1] = current;
    }

    for (int i = 0; i < count; i++) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            // Child:
            // Freeze
            raise(SIGSTOP);

            execvp(procs[i].args[0], procs[i].args);

            perror(procs[i].args[0]);
            exit(1);
        }

        // Parent:
        procs[i].pid = pid;
        waitpid(pid, NULL, WUNTRACED);
    }

    // IMPLEMENT ROUND ROBIN HERE
    // Currently: Just runs each process in priority order
    // Need to implement round robin still
    for (int i = 0; i < count; i++) {
        kill(procs[i].pid, SIGCONT);
        waitpid(procs[i].pid, NULL, 0);
    }

    for (int i = 0; i < count; i++) {
        for (int j = 0; j < procs[i].nargs; j++) {
            free(procs[i].args[j]);
        }
        free(procs[i].args);
    }
    free(procs);

    return 0;
}