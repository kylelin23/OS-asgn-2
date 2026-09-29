#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "schedule.h"
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>

volatile sig_atomic_t quantumDone = 0;

void alarm_handler(int signum){
    quantumDone = 1;
}

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

    /*Executes Sequentially
    for (int i = 0; i < count; i++) {
        kill(procs[i].pid, SIGCONT);
        waitpid(procs[i].pid, NULL, 0);
    }
    */

    //do not busy wait
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = alarm_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    printf("Total number of processes in the system: %d\n", count);
    int processesLeft =  count;
    int currentIndex = 0;

    // While there are still processes left in the array
    while (processesLeft > 0){
        // starts at -1 b/c highest priority is 0
        int highestPriority = -1;

        //Highest Priority in Array
        for (int i = 0; i < count; i++){
            if(!procs[i].done){
                if(highestPriority == -1 || procs[i].priority < highestPriority){
                    highestPriority = procs[i].priority;
                }
            }
        }
        //Looking for more processes that share highest priority
        int priorityCount = 0;
        for(int i = 0; i < count; i++){
            if(!procs[i].done && procs[i].priority == highestPriority){
                priorityCount++;
            }
        }
        //Execute if it matches highest priority
        for(int c = 0; c < count; c++){
            int i = (currentIndex + c) % count;
            if(procs[i].done || procs[i].priority != highestPriority){
                continue;
            }
            
            Process *p = &procs[i];
                //Only 1 process left at highestPriority
            //execute until completion and reduce the number of processes remaining
            if(priorityCount == 1){
                kill((*p).pid, SIGCONT);
                waitpid((*p).pid, NULL, 0);
                (*p).done = 1;
                processesLeft--;
                currentIndex = (i + 1) % count;
            }
            else{
                quantumDone = 0;
                //timer
                struct itimerval timer;

                timer.it_value.tv_sec = quantum / 1000;
                timer.it_value.tv_usec = (quantum % 1000) * 1000;

                timer.it_interval.tv_sec = 0;
                timer.it_interval.tv_usec = 0;

                setitimer(ITIMER_REAL, &timer, NULL);

                //wake up process
                kill((*p).pid, SIGCONT);

                int processStatus = 0;

                pid_t result = waitpid((*p).pid, &processStatus, 0);

                timer.it_value.tv_sec = 0;
                timer.it_value.tv_usec = 0;
                setitimer(ITIMER_REAL, &timer, NULL);

                /*
                if(result == 0){
                    result = waitpid((*p).pid, &processStatus, WNOHANG);
                }
                
                if(result > 0){
                    (*p).done = 1;
                    processesLeft--;
                }
                else if(result == 0){
                    kill((*p).pid, SIGSTOP);
                    waitpid((*p).pid, &processStatus, WUNTRACED);
                }
                else{
                    perror("waitpid");
                    exit(1);
                }
                    */
                if(result > 0){
                    (*p).done = 1;
                    processesLeft--;
                }
                else if (result < 0 && errno == EINTR && quantumDone){
                    kill((*p).pid, SIGSTOP);
                    waitpid((*p).pid, &processStatus, WUNTRACED);

                }
                else{
                    perror("waitpid");
                    exit(1);
                }
                currentIndex = (i + 1) % count; 
                break;
            }
        }
    
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



