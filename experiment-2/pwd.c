//
// Created by xgs on 22-12-8.
//
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

int main_pwd() {
    //get current dir .
    //keep access
    //record the final path
    char *result[255];
    int count = 0;
    DIR *dir;
    struct dirent *p;
    struct stat statbuf;
    while (1) {
        stat(".", &statbuf);
        u_int32_t curNodeNumber = statbuf.st_ino;
        stat("..", &statbuf);
        u_int32_t parentNodeNumber = statbuf.st_ino;
        if (curNodeNumber == parentNodeNumber) {
            break;
        }
        //else attempt to get the filename
        chdir("..");
        dir = opendir(".");
        while (NULL != (p = readdir(dir))) {
            if (p->d_ino == curNodeNumber) {
                result[count] = malloc(strlen(p->d_name) + 1);
                strcpy(result[count++], p->d_name);
                break;
            }
        }
        closedir(dir);
        //chdir to parent
    }
    //reverse and print
    printf("/");
    for (count = count - 1; count >= 0; count--) {
        printf("%s/", result[count]);
    }
    return 0;
}