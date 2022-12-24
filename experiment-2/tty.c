//
// Created by xgs on 22-12-8.
//
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

int main_tty(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: cmd [path]\n");
        exit(1);
    }
    int fd;
    char buf[1024];
    char *pLast = strstr(buf, "/dev/pts");
    if (NULL != pLast) {
        printf("invalid file path \n");
        exit(1);
    }
    if ((fd = open(argv[1], O_RDWR)) == -1) {
        perror("cannot open");
        exit(1);
    }

    write(fd, "hello world\n", 13);
    sleep(1);
    u_int32_t n;

    while (0 != (n = read(fd, buf, sizeof(buf)))) {
        printf("%.*s\n", n, buf);
    }
    close(fd);
}