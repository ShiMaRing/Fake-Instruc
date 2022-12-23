#include <sys/ipc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <sys/wait.h>

//定义队列长度为3
#define N 3
//生产数量
#define NUM 10
#define DATA_LEN 10


//产品结构体,用来存放数据
typedef struct Product {
    int id;
    char data[DATA_LEN];
} Product;

union semun {
    int val;    /* Value for SETVAL */
    struct semid_ds *buf;    /* Buffer for IPC_STAT, IPC_SET */
    unsigned short *array;  /* Array for GETALL, SETALL */
    struct seminfo *__buf;  /* Buffer for IPC_INFO(Linux-specific) */
};

typedef struct head {
    int consumer_p; //消费者位置
    int producer_p; //生产者位置
} head;

typedef struct meta_data {
    head *h;         //指向数据开始处
    char *data;       // 消费队列的起始位置
    int shmid;        //享内存id
    int mutex_id;     //共享内存的id,表示互斥锁，保护位置信息，保护内存中的数据
    int empty_id;     //共享内存的id,表示当前剩余消费数量，empty为0表示无法消费
    int full_id;      //共享内存的id,表示当前剩余生产数量，full为0表示无法生产
} meta;

meta *get_meta(key_t key);

//程序初始化
meta *create(key_t key) {
    meta *m = malloc(sizeof(meta));
    int len = sizeof(Product) * N + sizeof(meta);//获取需要开辟的空间长度
    int shmid;
    shmid = shmget(key, 0, 0);
    if (shmid != -1) { //之前已经开辟过了
        return get_meta(key);
    }
    shmid = shmget(key, len, IPC_CREAT | 0666);
    //开辟失败
    if (shmid == -1) {
        perror("shmgt");
        exit(EXIT_FAILURE);
    }
    //初始化信息
    m->h = shmat(shmid, NULL, 0);//将数据挂载到头部
    printf("m->h: %p \n", m->h);
    m->data = (char *) (m->h + 1);//将下一个地址指向数据起始地址
    printf("m->h+1: %p\n", m->h + 1);

    m->shmid = shmid;
    //获取信号量id
    m->mutex_id = semget(key, 1, IPC_CREAT | 0666);
    m->empty_id = semget(key + 1, 1, IPC_CREAT | 0666);
    m->full_id = semget(key + 2, 1, IPC_CREAT | 0666);

    //设置初始信号量
    union semun su = {1};
    semctl(m->mutex_id, 0, SETVAL, su);
    su.val = N;
    semctl(m->empty_id, 0, SETVAL, su);
    su.val = 0;
    semctl(m->full_id, 0, SETVAL, su);

    return m;
}

//获取已经开辟的共享内存
meta *get_meta(key_t key) {
    meta *m = malloc(sizeof(meta));
    int shmid = shmget(key, 0, 0666);
    m->h = shmat(shmid, NULL, 0);//将数据挂载到头部
    m->data = (char *) (m->h + 1);//将下一个地址指向数据起始地址
    m->shmid = shmid;
    //获取信号量,注意进程间共享
    m->mutex_id = semget(key, 1, 0666);
    m->empty_id = semget(key + 1, 1, 0666);
    m->full_id = semget(key + 2, 1, 0666);
    return m;
}

//原子减少信号量
void Decrease(int id) {
    struct sembuf buf = {0, -1, 0};
    semop(id, &buf, 1);
}

//原子添加信号量
void Add(int id) {
    struct sembuf buf = {0, 1, 0};
    semop(id, &buf, 1);
}

// 生产
void put_data(meta *m, Product *buf) {
    Decrease(m->empty_id);  //等待消费队列非空
    Decrease(m->mutex_id); //互斥锁
    //数据写入
    memcpy(m->data + m->h->producer_p * sizeof(Product),
           buf,
           sizeof(Product));

    //更新位置
    m->h->producer_p = (m->h->producer_p + 1) % N;
    Add(m->full_id);  //增加产品数量
    Add(m->mutex_id); //解锁
}

// 消费
void get_data(meta *m, Product *buf) {
    Decrease(m->full_id);  //减少可以消费的数量，检测队列是否为空
    Decrease(m->mutex_id); //互斥锁
    //读取数据
    memcpy(buf, m->data + m->h->consumer_p * sizeof(Product),
           sizeof(Product));
    //更新数据
    m->h->consumer_p = (m->h->consumer_p + 1) % N;
    Add(m->empty_id);  //增加可以生产的数量
    Add(m->mutex_id); //解锁
}

// 释放共享内存
void free_meta(meta *m) {
    shmdt(m->h);
    shmctl(m->shmid, IPC_RMID, 0);
    semctl(m->mutex_id, 0, IPC_RMID, 0);
    semctl(m->empty_id, 0, IPC_RMID, 0);
    semctl(m->full_id, 0, IPC_RMID, 0);
    free(m);
}

int main_consumer_producer() {
    //定义key
    key_t key = 123;
    pid_t child_pid;

    if ((child_pid = fork()) < 0) {
        perror("Fork Fail");
        exit(EXIT_FAILURE);
    }
    srand((unsigned) time(NULL));
    if (child_pid == 0) {
        meta *m = get_meta(key);
        sleep(1);//睡一会,让父进程开辟一下空间
        printf("start to consume \n");
        for (int i = 0; i < NUM; ++i) {
            Product p;
            get_data(m, &p);
            printf("(consume) id:%d  data:%s \n", p.id, p.data);
            sleep(1);
        }
        exit(0);
    } else {
        meta *m = create(key);
        for (int i = 0; i < NUM; ++i) {
            Product p;
            p.id = i;
            for (int j = 0; j < DATA_LEN; ++j) {
                p.data[j] = rand() % 26 + 'a';
            }
            printf("(produce) id:%d  data:%s \n", p.id, p.data);
            put_data(m, &p);
            sleep(1);
        }
        free_meta(m);
        int status;
        wait(&status);
    }

}