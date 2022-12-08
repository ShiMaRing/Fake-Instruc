//
// Created by xgs on 22-12-8.
//
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

int main_tty() {
    int fd;
    char buf[1024];
    if ((fd = open("/dev/pts/0", O_RDWR)) == -1) {
        perror("cannot open");
        exit(1);
    }
    write(fd, "please input:\n", 20);
    sleep(1);
    u_int32_t n;
    while (0 != (n = read(fd, buf, sizeof(buf)))) {
        printf("%.*s\n", n, buf);
    }
    close(fd);
}