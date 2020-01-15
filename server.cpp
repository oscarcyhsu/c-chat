#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <vector>


using namespace std;


typedef struct{
	int online,balance;
	char port[7];
	char host[50];
	char name[100];

}client;
typedef struct{
	int fd;
	char host[50];
}conn;
int online_num = 0;
vector<client> clis;
pthread_mutex_t m_lock = PTHREAD_MUTEX_INITIALIZER;
void* deal_with_client(void *cli);
int main(int argc, char **argv){

	if (argc!=3){
		printf("please enter: ./server [port][thread_num]");
		exit(1);
	}
	int listenfd,port;
	int thread_num = atoi(argv[2]);
	listenfd = socket(AF_INET,SOCK_STREAM,0);
	if(listenfd < 0){
		perror("socket()");
		exit(1);
	}
	port = atoi(argv[1]);
	if (port< 1024 || port > 65535)
	{
		printf("port must between 1024 ~ 65535");
		exit(1);
	}
	//addr sets
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if(bind(listenfd,(struct sockaddr*)&addr,sizeof(addr))<0){perror("bind: ");exit(1);}

	if(listen(listenfd,5)<0)
	{
		perror("listen: ");
		exit(1);
	}
	
	int newfd;

	struct sockaddr_in ac_addr;
	ac_addr.sin_family = AF_INET;
	ac_addr.sin_port = htons(port);
	ac_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	int ac_size = sizeof(ac_addr);

	while(1){
		
		if((newfd = accept(listenfd,(struct sockaddr*)&ac_addr,(socklen_t *) &ac_size)) < 0 ){
			perror("accept ");
			exit(1);
		}
		conn *con =(conn*) malloc(sizeof(conn));
		con->fd = newfd;
		strcpy(con->host,inet_ntoa(ac_addr.sin_addr));
		printf("new connection %d, %s\n",newfd,con->host);
		pthread_t pnum;
		pthread_create(&pnum,NULL,&deal_with_client,(void *)con);
		pthread_detach(pnum);

	}

}

void* deal_with_client(void *con){
	int fd = ((conn *) con) -> fd;
	char host[50];
	strcpy(host,((conn *) con) -> host);
	free(con);

	char buf[1000];

	//accept
	sprintf(buf,"connection accepted\n");
	if(write(fd,buf,strlen(buf))<0){perror("write"); return 0;}
	//register or sign
	int i,j,key,user_num = -1;
	char *name;
	
	while (user_num == -1)
	{
		int l;
		if((l = read(fd,buf,sizeof(buf))<= 0 )){perror("read");return 0;}
		
		//register
		if(buf[0] == 'R'){
			key =1;
			name = strtok(buf,"#");
			name = strtok(NULL,"\n");
			//lock mutex for check
			if(pthread_mutex_lock(&m_lock) < 0){perror("mutex_lock");return 0;}
			for(i=0;i<clis.size();i++)
			{
				if(strcmp(name,clis[i].name)==0)
				{
					key = 0;
					break;
				}
			}
			if(key){
				//set new client
				client cli;
				cli.online = 0;
				cli.balance = 1000;
				strcpy(cli.name,name);
				
				clis.push_back(cli);
				
				sprintf(buf,"100 OK\n");
				if(write(fd,buf,strlen(buf))<0) {perror("write");return 0;}
			}
			else{
				sprintf(buf,"210 FAIL\n");
				if(write(fd,buf,strlen(buf)) < 0 ){ perror("write"); return 0;}
			}
			//unlock after check and change
			if(pthread_mutex_unlock(&m_lock)< 0 ){perror("mutex_unlock");return 0;}
		}
		else {
			//sign
			char *port;
			name = strtok(buf,"#");
			port = strtok(NULL,"\n");
			if(pthread_mutex_lock(&m_lock) < 0){perror("mutex_lock");return 0;}
			for(i=0;i<clis.size();i++){
				if(strcmp(clis[i].name,name) == 0){
					//exist
					online_num++;
					user_num = i;
					clis[i].online = 1;
					strcpy(clis[i].port,port);
					strcpy(clis[i].host,host);
					sprintf(buf,"%d\n",clis[i].balance);
					if(write(fd,buf,strlen(buf)) < 0 ){ perror("write"); return 0;};
					sprintf(buf,"number of accounts online: %d\n",online_num);
					if(write(fd,buf,strlen(buf)) < 0 ){ perror("write"); return 0;}
					for(j = 0; j<clis.size();j++){
						if(clis[j].online){
							sprintf(buf,"%s#%s#%s\n",clis[j].name,clis[j].host,clis[j].port);
							if(write(fd,buf,strlen(buf)) < 0 ){ perror("write"); return 0;}
						}
					}
					break;
				}
			}
			if(pthread_mutex_unlock(&m_lock)< 0 ){perror("mutex_unlock");return 0;}
			if(user_num == -1){
				sprintf(buf,"220 AUTH_FAIL\n");
				if(write(fd,buf,strlen(buf)) < 0 ){ perror("write"); return 0;}
			}

		}
	}
	while(1){
		if(read(fd,buf,sizeof(buf))<0){perror("read"); return 0;}
		if(buf[0]=='L')
		{
			if(pthread_mutex_lock(&m_lock)< 0 ){perror("mutex_lock");return 0;}
			sprintf(buf,"%d\n",clis[user_num].balance);
			if(write(fd,buf,strlen(buf)) < 0 ){ perror("write"); return 0;};
			sprintf(buf,"number of accounts online: %d\n",online_num);
			if(write(fd,buf,strlen(buf)) < 0 ){ perror("write"); return 0;}
			for(j =0; j<clis.size();j++){
				if(clis[j].online){
					sprintf(buf,"%s#%s#%s\n",clis[j].name,clis[j].host,clis[j].port);
					if(write(fd,buf,strlen(buf)) < 0 ){ perror("write"); return 0;}
				}
			}
			if(pthread_mutex_unlock(&m_lock)< 0 ){perror("mutex_unlock");return 0;}
		}
		else if(buf[0] == 'E'){
			sprintf(buf,"Bye\n");
			if(pthread_mutex_lock(&m_lock)< 0 ){perror("mutex_lock");return 0;}
			online_num--;
			clis[user_num].online = 0;
			if(pthread_mutex_unlock(&m_lock)< 0 ){perror("mutex_unlock");return 0;}
			if(write(fd,buf,strlen(buf)) < 0 ){ perror("write"); return 0;};
			break;

		}
	}
	return 0;


}