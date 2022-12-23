#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>

#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
// epoll头文件
#include <sys/epoll.h>
#include <sys/resource.h>

#define MAXEPOLL 2000

void handle_404(int tcpfd);
void handle_get(int tcpfd, char *filename);
void handle_bad_request(int tcpfd);
void handle_request(int tcpfd, char *request);
void handle_back(int tcpfd);
int backCheck(char *filename);

int main(int ac, char *av[]){
	//socket文件描述符
	int sockfd;
	struct sockaddr_in addr;
	int fd;
	//buf用于读取client发送的消息
	char buf[1024];
	int n;
	signal(SIGPIPE, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	if(ac!=2){
		printf("Use: CMD PORT_NUM\n");
		exit(1);
	}
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0))==-1){
		//创建socket时出现问题
		perror("Cannot make socket");
		exit(1);
	}
	addr.sin_family=AF_INET;
	addr.sin_port=htons(atoi(av[1]));
	addr.sin_addr.s_addr=INADDR_ANY;
	if(bind(sockfd, (const struct sockaddr *)&addr, sizeof(struct sockaddr_in))==-1){
		//绑定socket文件描述符时发生问题
		perror("Cannot bind");
		exit(1);
	}
	//监听socket
	int listen_fd = listen(sockfd, 1);
	//epoll部分
	//设置监听为非阻塞模式
	if(fcntl(listen_fd, F_SETFL, fcntl(listen_fd, F_GETFD, 0)|O_NONBLOCK) == -1){
		printf("Setnonblocking Error : %d\n", errno);
		exit( EXIT_FAILURE );
	}

	int epoll_fd;  //epoll创建后的描述符
	int cur_fds;   //当前已经存在的数量
	int wait_fds;  // epoll_wait 的返回值
	int conn_fd;   //
	int i;         //循环用参数
	int j;         //循环用参数
	struct epoll_event ev;            //记录即将处理的epoll_event
	struct epoll_event evs[MAXEPOLL]; //等待处理的队列
	socklen_t len = sizeof(struct sockaddr_in);
	struct sockaddr_in cliaddr;
	//创建一个epoll
	epoll_fd = epoll_create(1);
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = sockfd; // 将sockfd加入ev中
	//注册要监听的事件类型：将socket文件描述符添加到epoll中
	if(epoll_ctl( epoll_fd, EPOLL_CTL_ADD, sockfd, &ev ) < 0){
		//注册监听事件发生错误
		printf("Epoll Error : %d\n", errno);
		exit(1);
	}
	cur_fds = 1; 
	while(1){
		//阻塞一段时间并等待事件发生，返回事件集合
		if((wait_fds = epoll_wait( epoll_fd, evs, cur_fds, -1)) == -1){
			printf( "Epoll Wait Error : %d\n", errno );
			exit(1);
		}
		for( i = 0; i < wait_fds; i++ ){
			//等待epoll事件从epoll实例中发生， 并返回事件以及对应文件描述符
			if(evs[i].data.fd == sockfd && cur_fds < MAXEPOLL){	
				if((conn_fd = accept(sockfd, (struct sockaddr *)&cliaddr, &len)) == -1 ){
					//建立连接时发生错误
					printf("Accept Error : %d\n", errno);
					exit(1);
				}
				printf("\33[32m-----Client connected success||有客户端连接到了服务器-----\33[37;0m\n");
				//记录当前的信息
				ev.events = EPOLLIN | EPOLLET;
				ev.data.fd = conn_fd;
				if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev ) < 0 ){
					printf("Epoll Error : %d\n", errno);
					exit(1);
				}
				cur_fds++; 
				continue;		
			}
			j = 0;
			//清空字符串，准备读取
			while(buf[j] != 0){
				buf[j] = 0;
				j++;
			}
			if((n = read(evs[i].data.fd, buf, sizeof(buf))) <= 0 ){
				//读取完成，关闭客户端输入
				close(evs[i].data.fd);
				epoll_ctl( epoll_fd, EPOLL_CTL_DEL, evs[i].data.fd, &ev); // 删除这一内容的fd
				cur_fds--; //记录数量减少一个
				continue;
			}
			//处理信息
			printf("\33[34;1m来自客户端的请求\n\33[37;0m%s\n",buf);
			handle_request(evs[i].data.fd, buf);
			close(evs[i].data.fd);
		}
	}
}


void handle_404(int tcpfd){
	int filefd = open("404.html", O_RDONLY);
	char buf[1024];
	int n;
	write(tcpfd, "HTTP/1.0 404 Not Found\r\n\r\n", strlen("HTTP/1.0 404 Not Found\r\n\r\n"));
	while((n=read(filefd, buf, sizeof(buf)))>0){
		write(tcpfd, buf, n);
	}
	close(filefd);
}

void handle_bad_request(int tcpfd){
	int filefd = open("400.html", O_RDONLY);
	char buf[1024];
	int n;
	write(tcpfd, "HTTP/1.0 400 Bad Request\r\n\r\n", strlen("HTTP/1.0 400 Bad Request\r\n\r\n"));
	while((n=read(filefd, buf, sizeof(buf)))>0){
		write(tcpfd, buf, n);
	}
	close(filefd);
}

void handle_back(int tcpfd){
	int filefd = open("bdr.html", O_RDONLY);
	char buf[1024];
	int n;
	write(tcpfd, "HTTP/1.0 400 Bad Request\r\n\r\n", strlen("HTTP/1.0 400 Bad Request\r\n\r\n"));
	while((n=read(filefd, buf, sizeof(buf)))>0){
		write(tcpfd, buf, n);
	}
	close(filefd);
}

void handle_get(int tcpfd, char *filename){
	if(backCheck(filename) == 1){
		handle_back(tcpfd);
		return;
	}
	int filefd;
	char buf[1024];
	int n;
	if((filefd=open(filename, O_RDONLY))==-1){
		handle_404(tcpfd);
	}else{
		write(tcpfd, "HTTP/1.0 200 OK\r\n\r\n", strlen("HTTP/1.0 200 OK\r\n\r\n"));
		while((n=read(filefd, buf, sizeof(buf)))>0){
			write(tcpfd, buf, n);
		}
		close(filefd);
	}
}

int backCheck(char *filename){
	int i = 0;
	int j = 1;
	//printf("%s\n",filename);
	while(filename[i] != '\0'){
		if(j==0 && filename[i]=='/'){
			j++;
		}else if((j==1||j==2) && filename[i]=='.'){
			j++;
		}else if(j==3 && filename[i]=='/'){
			return 1;
		}else {
			j = 0;
		}
		//printf("%d\n",j);
		i++;
	}
	return 0;
}

void handle_request(int tcpfd, char *request){
	char cmd[512];
	char filename[512];
	sscanf(request, "%s%s", cmd, filename);
	//printf("cmd: %s, filename: %s\n", cmd, filename);	
	if(strcmp(cmd, "GET")==0){
		handle_get(tcpfd, filename+1);
	}else{
		handle_bad_request(tcpfd);
	}
	close(tcpfd);
}


