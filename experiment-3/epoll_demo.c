//
// Created by xgs on 22-12-23.
//
#define MAX_EVENTS 50

#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>

struct request_param {
    int fd;
    char *filename;
};

void handle_get(int fd, char path[]) {
    char buf[1024];
    int n;
    int filefd;
    //remove the first filepath
    //path can not start with / or .
    printf("openfile: %s \n", path);
    if ((filefd = open(path, O_RDONLY)) == -1) {
        write(fd, "HTTP/1.0 404 Not Found\r\n\r\n", 26);
        filefd = open("404.html", O_RDONLY);
        while ((n = read(filefd, buf, sizeof(buf))) > 0) {
            write(fd, buf, n);
        }
        close(filefd);
        close(fd);
        return;
    }
    write(fd, "HTTP/1.0 200 OK\r\n\r\n", 19);
    while ((n = read(filefd, buf, sizeof(buf))) > 0) {
        write(fd, buf, n);
    }
    close(fd);
}

int valid(char *filename);

void *handle_thread(void *param) {
    struct request_param *p;
    p = (struct request_param *) param;
    pthread_detach(pthread_self());
    handle_get(p->fd, p->filename);
}


int valid(char *filename) {
    if (strlen(filename) < 1) {
        return -1;
    }
    char parent_path[2] = "..";
    char *p = strstr(filename, parent_path);
    if (p != NULL) {
        return -1;
    }
    //truncate the filename
    // /~/../xxx -> xxx
    int offest = 0;
    for (int i = 0; i < strlen(filename); ++i) {
        if (filename[i] == '.' || filename[i] == '/') {
            offest++;
        } else {
            break;
        }
    }
    return offest;
}

char filename[512];


int main(int ac, char *av[]) {
    struct epoll_event ev, events[MAX_EVENTS];
    char buf[2048];
    int n;
    char cmd[512];
    char path[512];
    pthread_t t;
    int listenfd, conn_sock, nfds, epollfd;
    struct sockaddr_in addr;
    signal(SIGPIPE, SIG_IGN);
    //create socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    //set addr,port
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(av[1]));
    addr.sin_addr.s_addr = INADDR_ANY;
    //bind socket to addr
    if (bind(listenfd, (const struct sockaddr *) &addr, sizeof(struct sockaddr_in)) == -1) {
        perror("cannot bind");
        exit(1);
    }
    //listen addr
    if (listen(listenfd, 5) == -1) {
        perror("listen fail");
        exit(EXIT_FAILURE);
    }
    //create epoll fd
    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }
    //set read in event
    ev.events = EPOLLIN | EPOLLET;;
    //set fd
    ev.data.fd = listenfd;
    //add to epoll pool
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listenfd, &ev) == -1) {
        perror("epoll_ctl: listenfd");
        exit(EXIT_FAILURE);
    }
    //keep listen
    for (;;) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, 500);
        if (nfds == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }
        for (n = 0; n < nfds; ++n) {
            //get new connection
            if (events[n].data.fd == listenfd) {
                //generate new conn_sock
                printf("accept connection, fd is %d\n", listenfd);
                conn_sock = accept(listenfd,
                                   NULL, NULL);

                if (conn_sock == -1) {
                    perror("accept new conn");
                    exit(EXIT_FAILURE);
                }

                int opts = fcntl(conn_sock, F_GETFL);
                opts = opts | O_NONBLOCK;
                //set the file descriptor to non-blocking and add it to the listener list
                fcntl(conn_sock, F_SETFL, opts);

                //set rw event and fd
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = conn_sock;
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock,
                              &ev) == -1) {
                    perror("epoll_ctl: conn_sock");
                    exit(EXIT_FAILURE);
                }
            } else if (events[n].events & EPOLLIN) {
                //something input
                int sockfd = events[n].data.fd;
                if (sockfd < 0) continue;
                if ((n = read(sockfd, buf, sizeof(buf))) < 0) {
                    if (errno == ECONNRESET) {
                        close(sockfd);
                        events[n].data.fd = -1;
                    }
                } else if (n == 0) {
                    close(sockfd);
                    events[n].data.fd = -1;
                }
                //deal with http request
                sscanf(buf, "%s%s", cmd, filename);
                ev.data.fd = sockfd;
                ev.events = EPOLLOUT | EPOLLET;
                epoll_ctl(epollfd, EPOLL_CTL_MOD, sockfd, &ev);
            } else if (events[n].events & EPOLLOUT) {
                int sockfd = events[n].data.fd;
                int offset = valid(filename + 1);
                if (strcmp(cmd, "GET") == 0 && offset != -1) {
                    int *fdbuf = malloc(sizeof(int));
                    *fdbuf = sockfd;
                    char tmp[512];
                    strcpy(tmp, filename + offset + 1);
                    struct request_param r;
                    r.filename = tmp;
                    r.fd = *fdbuf;
                    //开启多线程处理
                    pthread_create(&t, NULL, handle_thread, &r);
                } else {
                    write(sockfd, "HTTP/1.0 400 Bad Request\r\n\r\n", 26);
                    int filefd = open("400.html", O_RDONLY);
                    while ((n = read(filefd, buf, sizeof(buf))) > 0) {
                        write(sockfd, buf, n);
                    }
                    close(filefd);
                    close(sockfd);
                }
                events[n].data.fd = -1;
                //删除监听
                epoll_ctl(epollfd, EPOLL_CTL_DEL, sockfd, NULL);
            }
        }
    }
};

