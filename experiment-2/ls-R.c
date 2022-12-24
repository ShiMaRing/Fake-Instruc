//
// Created by xgs on 22-12-8.
//
#include <sys/types.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

void ls_dir(char *dirPath) {
    //argc=2 when user indicate dir
    DIR *dir;
    struct dirent *p;
    struct stat statbuf;
    printf("%s :\n", dirPath);
    //read dir info and show
    if (NULL == (dir = opendir(dirPath))) {
        perror("open dir fail");
        exit(1);
    }
    long previous_position = telldir(dir);
    //print first
    while (NULL != (p = readdir(dir))) {
        if (p->d_name[0] == '.') {
            continue;
        }
        printf("%s\t", p->d_name);
    }
    printf("\n\n");
    seekdir(dir, previous_position);
    while (NULL != (p = readdir(dir))) {
        if (p->d_name[0] == '.') {
            continue;
        }
        char *source_path = malloc(strlen(dirPath) + strlen(p->d_name) + 2);
        sprintf(source_path, "%s/%s", dirPath, p->d_name);
        stat(source_path, &statbuf);
        if (S_ISDIR(statbuf.st_mode)) {
            ls_dir(source_path);
        }
        free(source_path);
    }
    closedir(dir);
}

int main_lsR(int argc, char *argv[]) {

    if (argc != 2) {
        printf("USAGE: CMD [dir] \n");
        exit(EXIT_FAILURE);
    }
    if (argv[1][strlen(argv[1]) - 1] == '/') {
        argv[1][strlen(argv[1]) - 1] = '\0';
    }
    ls_dir(argv[1]);
    return 0;
}
