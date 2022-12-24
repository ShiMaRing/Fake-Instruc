#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

void file_copy(const char *src, const char *dst) {
    char buf[BUFFER_SIZE];
    u_int32_t n;
    FILE *input = fopen(src, "r");
    if (input == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    FILE *output = fopen(dst, "w");
    if (output == NULL) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }
    while ((n = fread(buf, 1, BUFFER_SIZE, input)) > 0) {
        fwrite(buf, 1, n, output);
    }
    fclose(input);
    fclose(output);
}

void dir_copy(const char *src, const char *dst) {

    DIR *dst_dir = opendir(dst);
    if (dst_dir == NULL) {
        if (mkdir(dst, 0666) != 0) {
            perror("mkdir");
            exit(EXIT_FAILURE);
        }
    } else {
        closedir(dst_dir);
    }
    DIR *src_dir = opendir(src);
    if (src_dir == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }
    struct dirent *dir;
    while ((dir = readdir(src_dir)) != NULL) {
        struct stat buf;
        if (dir->d_name[0] == '.') {
            continue;
        }
        char *src_path = malloc(strlen(src) + strlen(dir->d_name) + 2);
        sprintf(src_path, "%s/%s", src, dir->d_name);
        char *dst_path = malloc(strlen(dst) + strlen(dir->d_name) + 2);
        sprintf(dst_path, "%s/%s", dst, dir->d_name);
        if (stat(src_path, &buf) != 0) {
            perror("stat");
            exit(EXIT_FAILURE);
        }
        if (S_ISREG(buf.st_mode)) {
            file_copy(src_path, dst_path);
        } else if (S_ISDIR(buf.st_mode)) {
            dir_copy(src_path, dst_path);
        }
        free(src_path);
        free(dst_path);
    }
    closedir(src_dir);
}

int main_cp_R(int argc, char *argv[]) {
    if (argc != 3) {
        printf("USAGE: CMD [src] [dst] \n");
        exit(EXIT_FAILURE);
    }
    const char *src = argv[1];
    const char *dst = argv[2];
    struct stat buf;
    if (stat(src, &buf) != 0) {
        perror("stat");
        exit(EXIT_FAILURE);
    }

    //判断文件类型
    if (S_ISREG(buf.st_mode)) {
        file_copy(src, dst);
    } else if (S_ISDIR(buf.st_mode)) {
        //合法性判断
        DIR *dst_dir = opendir(dst);
        if (dst_dir == NULL) {
            if (mkdir(dst, 0666) != 0) {
                perror("mkdir");
                exit(EXIT_FAILURE);
            }
        } else {
            closedir(dst_dir);
        }
        char src_abs[100];
        char dst_abs[100];
        realpath(src, src_abs);
        realpath(dst, dst_abs);
        rmdir(dst);
        if (strstr(dst_abs, src_abs) != NULL) {
            printf("Error: src path include dst path\n");
            exit(1);
        }
        dir_copy(src, dst);
    }
}

