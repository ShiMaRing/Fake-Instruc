#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <pthread.h>

#define MSGKEY 99

pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;

//共享内存中存放的结构体
struct msgform {
	pid_t mtype;
	char mtext[50];
}msg;

void randomLetter(char* buf, int size);

int main_4_1(){
	//载入随机数种子
	srand((unsigned)time(NULL));
	int msgqid;
	//fpid是fork函数的返回值
	pid_t fpid = 1;
	//创建1个子进程
	if((fpid = fork())<0) printf("Error creating fork");
	if(fpid == 0){
		//子进程部分
		//子进程休眠1s以保证父进程先进入megrcv状态
		printf("\33[31mSonThread starts to send in 1 sec...\33[37;0m\n");		
		sleep(1);
		int i;
		msgqid=msgget(MSGKEY, 0777); //打开KEY为99的消息队列
		//子线程发送10条消息
		for(i=10; i >= 1; i--) {
			msg.mtype = i;
			randomLetter(msg.mtext, sizeof(msg.mtext));
			//设置同步锁解决并发问题（一个子线程时无作用）
			pthread_mutex_lock(&mutex_lock);
			printf("\33[34mSonThread send :       \33[37;0m%s\n",msg.mtext);
			msgsnd(msgqid, &msg, sizeof(struct msgform), 0); //发送消息
			pthread_mutex_unlock(&mutex_lock);
			//随机休眠500至1500ms
			usleep(((rand()%1001)+500)*1000);
		}
		exit(0);
	}else{
		//父进程部分
		//创建一个共享内存区
		msgqid=msgget(MSGKEY, 0777|IPC_CREAT);
		do{
			pthread_mutex_lock(&mutex_lock);
			msgrcv(msgqid, &msg, sizeof(struct msgform), 0, 0);  //接收子进程消息
			printf("\33[33mFatherThread receive : \33[37;0m%s\n",msg.mtext);
			pthread_mutex_unlock(&mutex_lock);
		}while(msg.mtype != 1);
		//释放设定的共享内存区
		msgctl(msgqid, IPC_RMID, 0);
		exit(0);
	}
}

//随机生成只含有大写字母的文本消息
void randomLetter(char* buf, int size){
	int i = 0;
	while(i<=size-2){
		buf[i] = 'A'+rand()%26;
		i++;
	}
	buf[size-1] = '\0';
}
