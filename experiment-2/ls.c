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

char *convertUint32toString(u_int32_t mode, char result[]) {
    for (int i = 0; i < 10; ++i) {
        result[i] = '-';
    }
    result[10] = '\0';

    switch (mode & S_IFMT) {
        case S_IFBLK:
            result[0] = 'b';
            break;
        case S_IFCHR:
            result[0] = 'c';
            break;
        case S_IFDIR:
            result[0] = 'd';
            break;
        case S_IFIFO:
            result[0] = 'F';
            break;
        case S_IFLNK:
            result[0] = 's';
            break;
        case S_IFREG:
            result[0] = 'r';
            break;
        case S_IFSOCK:
            result[0] = 's';
            break;
        default:
            result[0] = 'u';
            break;
    }
    if (S_IRUSR & mode) {
        result[1] = 'r';
    }
    if (S_IWUSR & mode) {
        result[2] = 'w';
    }
    if (S_IXUSR & mode) {
        result[3] = 'x';
    }

    if (S_IRGRP & mode) {
        result[4] = 'r';
    }
    if (S_IWGRP & mode) {
        result[5] = 'w';
    }
    if (S_IXGRP & mode) {
        result[6] = 'x';
    }

    if (S_IROTH & mode) {
        result[7] = 'r';
    }
    if (S_IWOTH & mode) {
        result[8] = 'w';
    }
    if (S_IXOTH & mode) {
        result[9] = 'x';
    }
    return result;
}

int main_ls(int argc, char *argv[]) {
    //argc=2 when user indicate dir
    char *dirPath;
    DIR *dir;
    struct dirent *p;
    char pathName[256];
    struct stat statbuf;
    char result[11];
    dirPath = "./";
    if (argc == 2) {
        dirPath = argv[1];
    } else if (argc > 2) {
        printf("USE CMD [path] \n");
        exit(1);
    }
    if (dirPath[strlen(dirPath) - 1] != '/') {
        strcat(dirPath, "/");
    }
    //read dir info and show
    if (NULL == (dir = opendir(dirPath))) {
        perror("open dir fail");
        exit(1);
    }

    int  total;
    long previous_position = telldir(dir);
    while (NULL != (p = readdir(dir))) {
        strcpy(pathName, dirPath);
        //p store info about dir
        if (p->d_name[0] != '.') {
            strcat(pathName, p->d_name);
            stat(pathName, &statbuf);
            total+=statbuf.st_blocks;
        }
    }
    printf("Total:%d\n",total/2);
    seekdir(dir, previous_position);

    while (NULL != (p = readdir(dir))) {
        strcpy(pathName, dirPath);
        //p store info about dir
        if (p->d_name[0] != '.') {
            strcat(pathName, p->d_name);
            if (0 == stat(pathName, &statbuf)) {
                u_int32_t size = statbuf.st_size;
                u_int32_t mode = statbuf.st_mode;
                u_int32_t nLink = statbuf.st_nlink;
                char *username = getpwuid(statbuf.st_uid)->pw_name;
                char *groupName = getgrgid(statbuf.st_gid)->gr_name;
                struct timespec lastEdit = statbuf.st_mtim;
                char *s = ctime(&lastEdit.tv_sec);
                s[strlen(s) - 1] = '\0';
                printf("%s %d %s %s %-5d %-11s \033[34m %s \033[0m \n", convertUint32toString(mode, result), nLink,
                       username, groupName, size,
                       s, p->d_name);
            } else {
                closedir(dir);
                perror("read file fail: ");
                exit(1);
            }
        }
    }
    closedir(dir);
    return 0;
}
