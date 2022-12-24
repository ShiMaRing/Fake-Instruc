//cp instruction implementation
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

int main_cp(int argc, char *argv[]) {
    //need two args
    const int MAX_BUF_LEN = 1024;
    const int FAIL = -1;
    char buf[MAX_BUF_LEN];
    if (argc != 3) {
        printf("usage: cmd [src] [dst]\n");
        exit(1);
    }
    char *src = argv[1];
    char *dst = argv[2];
    if (!strcmp(src, "") || !strcmp(dst, "")) {
        printf("empty arg\n");
        exit(1);
    }
    char *errMsg;
    int srcFd, dstFd;
    int access_res;
    if ((access_res = access(src, R_OK))) {
        perror("access file fail");
        exit(1);
    }

    if (access(dst, F_OK) == 0) {
        printf("File already exists! \n");
        exit(1);
    }

    if (FAIL == (srcFd = open(src, O_RDONLY, 0644))) {
        sprintf(errMsg, "open src file %s fail", src);
        perror(errMsg);
        exit(1);
    }
    if (FAIL == (dstFd = open(dst, O_WRONLY | O_CREAT, 0644))) {
        sprintf(errMsg, "open  dst file %s fail", src);
        perror(errMsg);
        exit(1);
    }

    int n;
    while ((n = read(srcFd, buf, MAX_BUF_LEN)) > 0) {
        write(dstFd, buf, n);
    }
    close(srcFd);
    close(dstFd);
    return 0;
}
