//
// Created by xgs on 22-12-23.
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/ipc.h>

#define MSGKEY 99

struct msgform {
    int mtype;
    char mtext[1000];
} msg;

int msgqid;

void server() {
    msgqid = msgget(MSGKEY, 0777 | IPC_CREAT);    /*创建KEY为99的消息队列*/
    do {
        msgrcv(msgqid, &msg, sizeof(struct msgform), 0, 0);   /*接收消息*/
        printf("server received\n");
    } while (msg.mtype != 1);
    msgctl(msgqid, IPC_RMID, 0);            /*删除消息队列*/
    exit(0);
}

int main() {
    server();
}