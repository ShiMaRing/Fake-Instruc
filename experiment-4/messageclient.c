//
// Created by xgs on 22-12-23.
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <sys/ipc.h>

#define MSGKEY 99

struct msgform {
    int mtype;
    char mtext[1000];
} msg;

int msgqid;

void client() {
    int i;
    msgqid = msgget(MSGKEY, 0777);        /*打开KEY为99的消息队列*/
    for (i = 10; i >= 1; i--) {
        msg.mtype = i;
        printf("client sent\n");
        msgsnd(msgqid, &msg, sizeof(struct msgform), 0);    /*发送消息*/
    }
    exit(0);
}

int main() {
    client();
}