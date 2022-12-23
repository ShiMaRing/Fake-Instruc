#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <grp.h>
#include <dirent.h>
#include <sys/types.h>

#define KEY_READ 99     //存放读取文件名的共享内存KEY
#define KEY_RESULT 93   //存放检查出存在关键字文件名的共享内存KEY
#define THREAD_NUM 10   //查找关键字进程的数量

sem_t * read_sem;            //POSIX信号量：共享内存KEY_READ中可以读取文件名数量
sem_t * read_empty_sem;      //POSIX信号量：共享内存KEY_READ中可以存放文件名数量
sem_t * result_sem;          //POSIX信号量：共享内存KEY_RESULT中可以读取结果数量
sem_t * result_empty_sem;    //POSIX信号量：共享内存KEY_RESULT中可以存放结果数量

pthread_mutex_t read_mutex_lock = PTHREAD_MUTEX_INITIALIZER;    //共享内存KEY_READ同步锁
pthread_mutex_t result_mutex_lock = PTHREAD_MUTEX_INITIALIZER;  //共享内存KEY_RESULT同步锁
pthread_mutex_t total_mutex_lock = PTHREAD_MUTEX_INITIALIZER;   //停止进程数量同步锁

int read_p_poi;     //向共享内存KEY_READ中存放文件名的指针
int read_g_poi;     //向共享内存KEY_READ中读取文件名的指针
int result_p_poi;   //向共享内存KEY_RESULT中存放文件名的指针
int result_g_poi;   //向共享内存KEY_RESULT中读取文件名的指针
int read_id;        //共享内存KEY_READ的id
int result_id;      //共享内存KEY_RESULT的id

int totalRead = 0;     //"/usr/include"目录下全部文件数量
int totalSearch = 0;   //查找关键字线程已经查找过的文件数量
int resultNum = 0;     //查找到含有关键字的文件数量
int resultPrint = 0;   //已经打印的含有关键字的文件数量
int stopThread = 0;    //已停止的线程数量

char text[256];       //存放关键字的字符串（传入参数改全局变量）

int countFile();                        //函数：统计"/usr/include"目录下的全部文件数量
void * pushInclude(void *arg);          //函数：向共享内存KEY_READ中写入"/usr/include"目录下文件线程（通过pushDir实现）
void pushDir(char *path);               //函数：向共享内存KEY_READ中写入"/usr/include"目录下文件的递归方法
int isDir(char *file);                  //函数：判断给出的文件是否是目录，执行pushDir()递归使用
void * finder(void *args);              //函数：查找线程，调用findText()方法并将结果存入KEY_RESULT
int findText(char * fileName, int i);   //函数：判断文件fileName是否含有关键字text，i是使用对应list的参数
void pushRead(char * fileName);         //函数：向共享内存KEY_READ中写入字符串
void getRead(char * buf);               //函数：从共享内存KEY_READ中读取字符串存到buf中
void pushResult(char * fileName);       //函数：向共享内存KEY_RESULT中写入字符串
void getResult(char * buf);             //函数：从共享内存KEY_RESULT中读取字符串存到buf中
void argumentsNotMatchTips();           //函数：提示参数不正确（仅参数为1时不正确）


char line[THREAD_NUM][128];             //函数findText的变量，避免反复开辟空间浪费时间和内存

int main_4_3(int ar, char *av[]){
	int i; //循环用变量
	if(ar == 1){
		argumentsNotMatchTips();
		return -1;
	}else if(ar >= 2){
		//拼接后续的参数，使其成为一条可以查找的文本（;、\n等无法拼接查找）
		strcpy(text, av[1]);
		for(i = 2; i < ar; i++){
			strcat(text," ");
			strcat(text,av[i]);
		}
	}
	char countPath[32];
	strcpy(countPath, "/usr/include");
	//strcpy(countPath, "/root/Documents/NeuAssignment/Experiment4/4-3/example");
	countFile(countPath);  //先查找需要查找的文件总数量
	//开辟共享内存read（KEY=99），用于存储需要被读取的文件名
	read_id = shmget(KEY_READ, 128, IPC_CREAT|0777);
	//开辟共享内存result（KEY=93），用于存储检查发现含有关键字的文件名
	result_id = shmget(KEY_RESULT, 128, IPC_CREAT|0777);
	//初始化共享内存（不必要）
	char init[10][128];
	for(i = 0; i<10; i++){
		strcpy(init[i], "\0");
	}
	char * cache;
	cache = shmat(read_id, NULL, 0);
	memmove(cache,init,1280);	
	/*for(i = 0; i<10; i++){
		printf("%s\n", cache + i*128);
	}*/
	cache = shmat(result_id, NULL, 0);
	memmove(cache,init,1280);
	//创建POSIX信号灯
	read_sem = sem_open("read", O_CREAT, 0644, 0);
	read_empty_sem = sem_open("read_empty", O_CREAT, 0644, 0);
	result_sem = sem_open("result", O_CREAT, 0644, 0);
	result_empty_sem = sem_open("result_empty", O_CREAT, 0644, 0);
	//POSIX信号量初始化（使用sem_open函数最后一个参数传入会发生问题）
	sem_init(read_sem, 0, 0);
	sem_init(read_empty_sem, 0, 10);
	sem_init(result_sem, 0, 0);
	sem_init(result_empty_sem, 0, 10);
	//初始化同步锁
	pthread_mutex_init(&read_mutex_lock, NULL);
	pthread_mutex_init(&result_mutex_lock, NULL);
	pthread_mutex_init(&total_mutex_lock, NULL);
	
	/*开始执行查找任务*/
	//开始向KEY_READ共享内存存入需要查找的文件名
	pthread_t push_include;
	pthread_create(&push_include, NULL, pushInclude, NULL);
	//初始化查找线程，数量为THREAD_NUM个
	pthread_t find[THREAD_NUM];
	for(i = 0; i<THREAD_NUM; i++){
		pthread_create(&find[i], NULL, finder, (void *)&i);
		usleep(200); //休眠0.2ms以供变量执行拷贝操作
	}
	//接收finder查找关键字到并存入KEY_RESULT的文件名
	char filePath[128];
	while(1){
		getResult(filePath);
		if(filePath[0] == '\0') break; //查找过程已经执行完成，程序退出
		printf("%s\n",filePath);
	}
}

//函数：统计"/usr/include"目录下的全部文件数量
int countFile(char * path){
	strcat(path, "/");
	DIR *dp;
	struct dirent *sdp;
	struct stat buf;
	//打开目录
	if((dp=opendir(path))==NULL){
		perror("Cannot open");
		exit(1) ;
	}
	char pushFile[256];
	while((sdp=readdir(dp))!=NULL){
		//对每一个普通文件进行计数
		if(sdp->d_name[0]!='.'){
			strcpy(pushFile, path);
			strcat(pushFile, sdp->d_name);
			if(!isDir(pushFile)){
				totalRead++;
			}
		}
	}
	closedir(dp);
	dp=opendir(path);
	while((sdp=readdir(dp))!=NULL){
		//对每一个文件夹进行递归计数
		if(sdp->d_name[0]!='.'){
			//拼接字符串为源和目标目录
			strcpy(pushFile, path);
			strcat(pushFile, sdp->d_name);
			if(isDir(pushFile)){
				strcpy(pushFile, path);
				strcat(pushFile, sdp->d_name);
				countFile(pushFile);//执行递归
			}
		}
	}
}

//函数：向共享内存KEY_READ中写入"/usr/include"目录下文件线程（通过pushDir实现）
void * pushInclude(void *arg){
	char path[128];
	strcpy(path, "/usr/include");
	//strcpy(path, "/root/Documents/NeuAssignment/Experiment4/4-3/example");
	pushDir(path);
}

//函数：向共享内存KEY_READ中写入"/usr/include"目录下文件的递归方法
void pushDir(char *path){
	strcat(path, "/");
	DIR *dp;
	struct dirent *sdp;
	struct stat buf;
	//打开目录
	if((dp=opendir(path))==NULL){
		perror("Cannot open");
		exit(1) ;
	}
	char pushFile[256];
	while((sdp=readdir(dp))!=NULL){
		//对每一个普通文件进行复制
		if(sdp->d_name[0]!='.'){
			strcpy(pushFile, path);
			strcat(pushFile, sdp->d_name);
			if(!isDir(pushFile)){
				//printf("Push: %s\n", pushFile);
				pushRead(pushFile);
			}
		}
	}
	closedir(dp);
	dp=opendir(path);
	while((sdp=readdir(dp))!=NULL){
		//对每一个文件夹进行递归复制
		if(sdp->d_name[0]!='.'){
			//拼接字符串为源和目标目录
			strcpy(pushFile, path);
			strcat(pushFile, sdp->d_name);
			if(isDir(pushFile)){
				strcpy(pushFile, path);
				strcat(pushFile, sdp->d_name);
				pushDir(pushFile);//执行递归
			}
		}
	}
}

//函数：判断给出的文件是否是目录，执行pushDir()递归使用
int isDir(char *file){
	struct stat status;
	stat(file, &status);
	return S_ISDIR(status.st_mode);
}

//函数：查找线程，调用findText()方法并将结果存入KEY_RESULT
void * finder(void *arg){
	int p = *((int *)arg);	
	while(1){
		char fileName[128];
		getRead(fileName);
		//printf("Pull:%s\n",fileName);
		//printf("%d",fileName[0]);
		if(fileName[0] == '\0'){
			break; //获取到了"\0"说明全部文件已经完成进入了查找操作
		}
		if(findText(fileName, p)){
			//向共享内存放入此文件名，以供主线程打印
			pushResult(fileName);
		}
	}
	//结束时是统计stopThread数量，达到THREAD_NUM说明全部完成
	pthread_mutex_lock(&total_mutex_lock);
	stopThread++;
	//printf("%d",stopThread);
	if(stopThread == THREAD_NUM){
		sem_post(result_sem);
	}
	pthread_mutex_unlock(&total_mutex_lock);
}

//函数：判断文件fileName是否含有关键字text，i是使用对应list的参数
int findText(char * fileName, int i){
	//采用fgets()方法按行读取内容
	FILE * fp=fopen(fileName,"r");
	while (fgets(line[i], 128, fp)){
		//使用strstr匹配关键字，不为空则说明存在，直接返回
		char *pLast = strstr(line[i], text);
		if (NULL != pLast){
			fclose(fp);
			return 1;
		}
	}
	fclose(fp);
	return 0;
}

//函数：向共享内存KEY_READ中写入字符串
void pushRead(char * fileName){
	sem_wait(read_empty_sem);
	char * buff;
	//设置同步锁解决并发问题
	pthread_mutex_lock(&read_mutex_lock);
	//取出共享内存中的数据
	buff = shmat(read_id, NULL, 0);
	//printf("read_p_poi:%d\n",read_p_poi);
	usleep(1000);
	strcpy(buff+read_p_poi*128,fileName);
	read_p_poi = (read_p_poi == 9)? 0 :read_p_poi+1;
	pthread_mutex_unlock(&read_mutex_lock);
	sem_post(read_sem);
}

//函数：从共享内存KEY_READ中读取字符串存到buf中
void getRead(char * buf){
	if(totalSearch == totalRead){
		strcpy(buf, "\0");
		sem_post(read_sem);
		return;
	}
	sem_wait(read_sem);
	if(totalSearch == totalRead){
		strcpy(buf, "\0");
		sem_post(read_sem);
		return;
	}else{
		char * buff;
		pthread_mutex_lock(&read_mutex_lock);
		//取出共享内存中的数据
		//printf("%d ,%d\n",totalSearch,totalRead);
	buff = shmat(read_id, NULL, 0);
	//printf("read_g_poi:%d\n",read_g_poi);
	strcpy(buf, buff+read_g_poi*128);
	read_g_poi = (read_g_poi == 9)? 0 :(read_g_poi+1);
	totalSearch += 1;
	pthread_mutex_unlock(&read_mutex_lock);
	}
	sem_post(read_empty_sem);
}

//函数：向共享内存KEY_RESULT中写入字符串
void pushResult(char * fileName){
	sem_wait(result_empty_sem);
	char * buff;
	//设置同步锁解决并发问题
	pthread_mutex_lock(&result_mutex_lock);
	//取出共享内存中的数据
	buff = shmat(result_id, NULL, 0);
	strcpy(buff+result_p_poi*128,fileName);
	result_p_poi = (result_p_poi == 9?0:result_p_poi+1);
	resultNum++;
	//printf("resultNum:%d\n",resultNum);
	pthread_mutex_unlock(&result_mutex_lock);
	sem_post(result_sem);
}

//函数：从共享内存KEY_RESULT中读取字符串存到buf中
void getResult(char * buf){
	sem_wait(result_sem);
	if(resultNum == resultPrint && stopThread == THREAD_NUM){
		strcpy(buf, "\0");
		return;
	}
	char * buff;
	//设置同步锁解决并发问题
	pthread_mutex_lock(&result_mutex_lock);
	//取出共享内存中的数据
	buff = shmat(result_id, NULL, 0);
	strcpy(buf, buff+result_g_poi*128);
	result_g_poi = (result_g_poi == 9?0:result_g_poi+1);
	resultPrint++;
	pthread_mutex_unlock(&result_mutex_lock);
	sem_post(result_empty_sem);
}

//函数：提示参数不正确（仅参数为1时不正确）
void argumentsNotMatchTips(){
	printf("Failed to match arguments.\n");
        printf("Arguments provided should be:\n");
        printf("  >=1 args: Keywords that you want to find.\n");
}