#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <time.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <fcntl.h>

//！不要设置数据过大，否则程序会发生崩溃！
/*更改本部分内容实现参数调节*/

#define MAXP 5           //生产者数量
#define MAXC 5           //消费者数量
#define BUF_SIZE 6       //产品存放空间
#define KEY 99           //共享内存标识符
#define TEXT_SIZE 31     //产品的随机文本长度
#define DIP 2000         //消费者在生产者之后启动的时间（单位：ms）

#define P_BASIC 500      //生产者最低生产时间（单位：ms）
#define P_NOISE 1000     //生产者生产波动时间（单位：ms）
#define C_BASIC 500      //消费者最低生产时间（单位：ms）
#define C_NOISE 1000     //消费者生产波动时间（单位：ms）

//结构体：产品
struct product{
	int producer_no;        //生产线程的编号
	char text[TEXT_SIZE];   //随机文本
};

int p_poi;    //生产者产品放置指针
int c_poi;    //消费者产品消费指针
int seg_id;   //共享内存id

sem_t * empty_block;  //POSIX信号量：产品存放空间的空闲位置数量
sem_t * exist_block;  //POSIX信号量：产品存放空间的产品数量
pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;  //写入、读取同步锁

void *produce(void *arg); //生产者生产函数
void *consume(void *arg); //消费者消费函数
void randomLetter(char* buf);  //随机文本生成函数
void clearLetter(char* buf);   //文本清空函数


main(int ar, char *av[]){
	pthread_t producer[MAXP]; //生产者线程组
	pthread_t consumer[MAXC]; //消费者线程组
	struct product * buf;     //共享内存的指针
	struct product empty[BUF_SIZE];  //用于向共享内存拷贝数据（初始化共享内存）
	int i,j;
	//载入随机数种子
	srand((unsigned)time(NULL));
	//开辟共享内存
	seg_id = shmget(KEY, 128, IPC_CREAT|0777);
	//创建POSIX信号灯
	empty_block = sem_open("empty", O_CREAT, 0644, 0);
	exist_block = sem_open("exist", O_CREAT, 0644, 0);
	//POSIX信号量初始化（使用sem_open函数最后一个参数传入会发生问题）
	sem_init(empty_block, 0, BUF_SIZE);
	sem_init(exist_block, 0, 0);
	//初始化同步锁
	pthread_mutex_init(&mutex_lock, NULL);
	
	//初始化生产者、消费者指针
	p_poi = 0;
	c_poi = 0;
	//对empty进行初始化
	empty[0] = (struct product){-1,"\0"};
	for(i = 1; i<BUF_SIZE; i++){
		memcpy(&empty[i], &empty, sizeof(struct product));
	}
	//将empty拷贝到共享内存buf中
	//获取共享内存地址
	buf = (struct product *)shmat(seg_id, NULL, 0);
	memmove(buf, empty, sizeof(struct product)*BUF_SIZE+1);
	/*for(i = 0; i<BUF_SIZE; i++){
		printf("%d %s",buf[i].producer_no,buf[i].text);
	}*/
	printf("\33[34mSystem Tips: basic struction complete.\n\33[37;0m");
	printf("\33[34mSystem Tips: produce threads starts.\n\33[37;0m");
	//创建生产者进程
	for(i = 0; i < MAXP; i++){
		int p = i+1;
		pthread_create(&producer[i], NULL, produce, (void *)&p);
		usleep(2000); //进行短时间的休眠，使刚生成的线程完成深拷贝操作
	}
	usleep(DIP*1000);  //时间差，用于生产者生产首批产品
	printf("\33[34mSystem Tips: consume threads starts.\n\33[37;0m");
	//创建消费者进程
	for(i = 0; i < MAXC; i++){
		int p = i+1;
		pthread_create(&consumer[i], NULL, consume, (void *)&p);
		usleep(2000); //进行短时间的休眠，使刚生成的线程完成深拷贝操作
	}
	//printf("\33[34mSystem Tips: threads will start in 3 sec.\n\33[37;0m");
	//生产者线程加入main()阻塞
	for(i = 0; i < MAXP; i++){
		pthread_join(producer[i],NULL);
	}
	//消费者线程加入main()阻塞
	for(i = 0; i < MAXC; i++){
		pthread_join(consumer[i],NULL);
	}
}

//生产者生产函数
void *produce(void *arg){
	int p = *((int *)arg);
	struct product * buff;
	//printf("%d\n",p);
	while(1){
		//生产
		//随机休眠
		usleep(((rand()%P_NOISE)+P_BASIC)*1000);
		//向empty_block发出wait信息，检查是否可以向产品存放区放入产品
		sem_wait(empty_block);
		//设置同步锁解决并发问题
		pthread_mutex_lock(&mutex_lock);
		//取出共享内存中的数据
		buff = shmat(seg_id, NULL, 0);
		buff[p_poi].producer_no = p;
		randomLetter(buff[p_poi].text);
		printf("\33[32mProduce\33[37;0m: PThr%d %s  to place %d\n",buff[p_poi].producer_no,buff[p_poi].text,p_poi);
		p_poi == BUF_SIZE-1?p_poi = 0:p_poi++;
		if(p_poi == c_poi) printf("\33[33mSystem Tips: \33[37;0mproduct cache \33[32mfull\33[37;0m.\n");
		pthread_mutex_unlock(&mutex_lock);
		//向exist_block发出post信息，说明有产品可以消费
		sem_post(exist_block);

	}
}

//消费者消费函数
void *consume(void *arg){
	int p = *((int *)arg);
	struct product * buff;
	//printf("%d\n",p);
	while(1){
		//消费
		//随机休眠500至1500ms
		usleep(((rand()%C_NOISE)+C_BASIC)*1000);
		//向exist_block发出wait信息，检查是否有产品可以消费
		sem_wait(exist_block);
		//设置同步锁解决并发问题
		pthread_mutex_lock(&mutex_lock);		
		//取出共享内存中的数据
		buff = shmat(seg_id, NULL, 0);
		printf("\33[31mConsume\33[37;0m: CThr%d %s  from place %d produced by %d\n",p,buff[c_poi].text,c_poi,buff[c_poi].producer_no);
		buff[c_poi].producer_no = -1;
		clearLetter(buff[c_poi].text);
		c_poi == BUF_SIZE-1?c_poi = 0:c_poi++;
		if(p_poi == c_poi) printf("\33[33mSystem Tips: \33[37;0mproduct cache \33[31mempty\33[37;0m.\n");
		pthread_mutex_unlock(&mutex_lock);
		//向empty_block发出post信息，说明可以向产品存放区放入产品
		sem_post(empty_block);
	}	
		
}



//随机生成只含有大写字母的文本消息
void randomLetter(char* buf){
	int i = 0;
	while(i<=TEXT_SIZE-2){
		buf[i] = 'A'+rand()%26;
		i++;
	}
	buf[TEXT_SIZE-1] = '\0';
}

//清空字符串
void clearLetter(char* buf){
	int i = 0;
	while(i<=TEXT_SIZE-1){
		buf[i] = '\0';
		i++;
	}
}
