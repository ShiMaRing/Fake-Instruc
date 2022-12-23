#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <pthread.h>

//定义消息长度
#define MSGSIZE 10
//主题
#define MSGKEY 99

//发送的消息数量
#define MSGNUM 10

struct msgform {
    int mtype;
    char mtext[MSGSIZE];
};

int main_SystemV() {
    int msg_qid;
    pid_t child_pid;
    int n;//指定发送数量
    //每一个进程都将复制该消息作为自己的buf
    struct msgform buf;
    //生成子进程
    if ((child_pid = fork()) < 0) {
        perror("Fork Fail");
        exit(EXIT_FAILURE);
    };
    srand((unsigned) time(NULL));
    if (child_pid == 0) {
        sleep(1);
        //执行权限被忽略，只有读写权限
        msg_qid = msgget(MSGKEY, 0666);
        for (n = MSGNUM; n >= 1; n--) {
            buf.mtype = n;
            for (int i = 0; i < MSGSIZE; ++i) {
                buf.mtext[i] = 'a' + rand() % 26;//随即设置
            }
            //发送消息,忽略错误并且无阻塞
            printf("(client)send: %s , msgtype:%d\n", buf.mtext, buf.mtype);
            msgsnd(msg_qid, &buf, MSGSIZE, MSG_NOERROR | IPC_NOWAIT);
        }
        exit(EXIT_SUCCESS);
    } else {
        //父进程创建消息队列
        msg_qid = msgget(MSGKEY, 0666 | IPC_CREAT);
        for (;;) {
            msgrcv(msg_qid, &buf, MSGSIZE, 0, 0);
            printf("(server)receive: %s , msgtype:%d\n", buf.mtext, buf.mtype);
            if (buf.mtype == 1) {
                break;
            }
        }
        //删除消息队列
        msgctl(msg_qid, IPC_RMID, NULL);
        exit(EXIT_SUCCESS);
    }
}
