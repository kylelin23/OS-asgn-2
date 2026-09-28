#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Please include two parameters: time quantum and file. ")
        return 1;
    }

    return 0;
}