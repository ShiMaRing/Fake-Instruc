//
// Created by xgs on 22-12-8.
//
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

int main() {
    char abspath[1024];
    const char *currentPath = ".";
    char *point = realpath(currentPath, abspath);
    if (point == NULL) {
        perror("calculate absolute path fail");
        exit(EXIT_FAILURE);
    }
    printf("\n%s\n", abspath);
    return 0;
}