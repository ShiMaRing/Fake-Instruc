
///多进程的模式匹配
///思路，使用消息队列进行处理
///父进程先开一个线程去递归遍历，并且把消息发送到消息队列1中
///父进程开十个子进程处理消息队列中的消息，然后把处理结果送入消息队列2
///父进程开一个线程去打印输出
///各个进程一旦接收到该信号，并且消息队列为空，则结束自身，父进程等待所有的子进程结束
///之后需要睡一会等待消费完所有的信息，进程退出程序结束

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/wait.h>

//文件名消息队列key
#define FILEPATH_QUEUE_KEY 99
//结果列表消息队列key
#define RESULT_QUEUE_KEY 100
//文件路径的最大长度
#define MSG_SIZE 256

#define FORK_NUM 10
typedef struct msg {
    int mtype;
    char mtext[MSG_SIZE];
} msg;

int path_queue;
int result_queue;
const char *EOF_SYMBOL = "EOF";

void dir_traverse(const char *dir_path);

//遍历文件
void *file_traverse(void *arg) {
    pthread_detach(pthread_self());
    const char *start_path = "/usr/include";
    //使用递归遍历
    dir_traverse(start_path);
    //执行完成需要发送结束标志符号，接收到结束符号直接就可以结束
    for (int i = 0; i < FORK_NUM; ++i) {
        msg m;
        strcpy(m.mtext, EOF_SYMBOL);
        msgsnd(path_queue, &m, MSG_SIZE, 0);
    }
}

//递归遍历文件，如果是目录就继续执行
//如果是文件，就提取路径发送到消息队列去
void dir_traverse(const char *dirPath) {
    DIR *dir;
    struct dirent *p;
    struct stat statbuf;
    //read dir info and show
    if (NULL == (dir = opendir(dirPath))) {
        perror("open dir fail");
        exit(1);
    }
    while (NULL != (p = readdir(dir))) {
        if (p->d_name[0] == '.') {
            continue;
        }
        //推送消息
        msg m;
        sprintf(m.mtext, "%s/%s", dirPath, p->d_name);
        stat(m.mtext, &statbuf);
        if (S_ISDIR(statbuf.st_mode)) {
            //是文件夹继续遍历
            dir_traverse(m.mtext);
        } else {
            //发送消息,以阻塞的方式
            msgsnd(path_queue, &m, MSG_SIZE, 0);
        }
    }
    closedir(dir);
}

void *output(void *arg) {
    pthread_detach(pthread_self());
    msg m;
    while (1) {
        int result = msgrcv(result_queue, &m, MSG_SIZE, 0, 0);
        //消息通道会被关闭，此时会直接返回错误
        if (result == -1) {
            break;
        }
        printf("%s\n", m.mtext);
    }

}

int main(int ac, char *args[]) {
    char keyword[64];
    int n = 0;
    int keyword_len = 0;
    if (ac == 1) {
        printf("USAGE:CMD [keyword] ");
        exit(EXIT_FAILURE);
    } else if (ac >= 2) {
        strcpy(keyword, args[1]);
        keyword_len += strlen(args[1]);
        for (n = 2; n < ac; n++) {
            strcat(keyword, " ");
            strcat(keyword, args[n]);
            keyword_len += strlen(args[n]);
            if (keyword_len >= 64) {
                printf("max key word length:64 ");
                exit(EXIT_FAILURE);
            }
        }
    }
    //文件遍历线程
    pthread_t filepath_traverse;
    //消息输出线程
    pthread_t result_output;
    int i;
    pid_t rv_fork;
    path_queue = msgget(FILEPATH_QUEUE_KEY, IPC_CREAT | 0666);
    if (path_queue == -1) {
        perror("path_queue:msgget");
        exit(EXIT_FAILURE);
    }
    result_queue = msgget(RESULT_QUEUE_KEY, IPC_CREAT | 0666);
    if (result_queue == -1) {
        perror("result_queue:msgget");
        exit(EXIT_FAILURE);
    }
    //开启线程准备读取文件
    pthread_create(&filepath_traverse, NULL, file_traverse, NULL);
    //开启线程读取消息并输出
    pthread_create(&result_output, NULL, output, NULL);

    //创造子进程
    for (i = 0; i < FORK_NUM; i++) {
        if ((rv_fork = fork()) == -1) {
            perror("Cannot fork");
            continue;
        } else if (0 == rv_fork) {
            //子进程不允许再创造
            break;
        }
    }
    if (rv_fork == 0) {
        //子进程代码块，需要读取数据，并且处理
        msg m;
        char buf[256];
        while (1) {
            int result = msgrcv(path_queue, &m, MSG_SIZE, 0, 0);
            //消息通道会被关闭，此时会直接返回错误
            if (result == -1) {
                break;
            }
            //读取到EOF直接退出
            if (strcmp(m.mtext, EOF_SYMBOL) == 0) {
                break;
            } else {
                //读取文件并进行关键字查找
                FILE *fp = fopen(m.mtext, "r");
                if (fp == NULL) {
                    perror("fopen");
                    break;
                }
                while (fgets(buf, 128, fp)) {
                    char *pLast = strstr(buf, keyword);
                    if (NULL != pLast) {
                        fclose(fp);
                        msgsnd(result_queue, &m, MSG_SIZE, 0);
                    }
                }
                fclose(fp);
            }
        }
        exit(EXIT_SUCCESS);
    } else {
        //wait all child
        int status;
        wait(&status);
        msgctl(path_queue, IPC_RMID, 0);
        //等待剩下的输出消息
        usleep(100);
        msgctl(result_queue, IPC_RMID, 0);
        exit(EXIT_SUCCESS);
    }
}



