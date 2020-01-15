#include <stdio.h>
#include <unistd.h>
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
int reg(int sockfd);
int sign(int sockfd);
int bye(int sockfd);
void handle_list(char * buf,int sockfd,int read_len);
void* wait_for_pay(void *listenfd){
    long long lifd = (long long)listenfd;
    if(listen((int)lifd,10) < 0)
        perror("listen: ");

}

int main(int argc,char **argv)
{
	if(argc != 3)
	{
		printf("Error, enter [ip] [port]\n");
		exit(0);
	}

	printf("Conneting with the server, please wait.\n");
	int sockfd;
	
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[2]));
	addr.sin_addr.s_addr = inet_addr(argv[1]);
	if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket error:");
		exit(0);
	}
	if(connect(sockfd,(struct sockaddr *)&addr,sizeof(struct sockaddr)) < 0)
	{
		perror("Connetion error:");
		exit(0);
	}
	int key = 1;
	char input[100],buf[1000];
	if(read(sockfd,buf,1000) < 0){
		perror("connetion error:");
	}
		
	

	
	while(key)
	{
		printf("enter the number (0:register,1:sign in):\n");
		scanf("%s",input);
		if(strlen(input)!=1 || (input[0] != '0' && input[0] != '1')){
			printf("enter the number in the right way(0:register, 1:sign in)!!!\n");
		}
		else if(input[0] == '0')
			key = reg(sockfd);
		else if(input[0] == '1')
			key = sign(sockfd);

		
	}
	printf("Enter (Q: quit, U: update the list):\n");
    while(scanf("%s",input)){
        if(strlen(input) == 1)
        {
            
            if(input[0]=='Q')
                bye(sockfd);
            else if(input[0] == 'U'){
                sprintf(buf,"List\n");
                if(write(sockfd,buf,strlen(buf)) <= 0){perror("error while writing to server:");exit(0);}
				int read_len;
                if((read_len = read(sockfd,buf,1000)) <= 0){perror("Connection abort:");exit(0);}
                handle_list(buf,sockfd,read_len);

            }
        }
		printf("Enter (Q: quit, U: update the list):\n");
    }




}

int reg(int sockfd){
	char input[50];
	int key = 1;

	while(key)
	{
		printf("Enter the name you want to register(2~20 number or alphebat):\n");
		scanf("%s",input);
		int i,len = strlen(input);
		if(len >= 2 && len <= 50)
		{
			key = 0;
			for(i =0;i < len;i++)
			{
				if(!(input[i]>='0' && input[i] <= '9' ||
					 input[i] >= 'a' && input[i] <= 'z' ||
					 input[i] >= 'A' && input[i] <= 'Z'))
				{
					key = 1;
					printf("number or alphetbat only\n\n");
					break;
				}
			}
		}
		else
			printf("2~20 number or char\n\n");
	}
	char buf[100];
	sprintf(buf,"REGISTER#%s\n",input);

	if(write(sockfd,buf,strlen(buf))<0){
		perror("connection abort:");
		exit(0);
	}
	
	if(read(sockfd,buf,100) <= 0){
		perror("server abort:");
		exit(0);
	}
	if(buf[0] == '1'){
		printf("register sucess\n\n");
	}
	else{
		printf("register failed\n\n");
	}
	return 1;

}

int sign(int sockfd){
	
	
	
    char input[50];

    int key = 1;
	while(key)
	{
		printf("Enter your name(2~20 number or alphebat):\n");
		scanf("%s",input);
		int i,len = strlen(input);
        
		if(len >= 2 && len <= 50)
		{
			key = 0;
			for(i =0;i < len;i++)
			{
				if(!(input[i]>='0' && input[i] <= '9' ||
					 input[i] >= 'a' && input[i] <= 'z' ||
					 input[i] >= 'A' && input[i] <= 'Z'))
				{
					key = 1;
					printf("number or alphetbat only!!\n\n");
					break;
				}
			}
		}
		else
			printf("2~20 number or char!\n\n");
	}
    key = 1;
    char port[20];
    while(key){
        printf("Enter the port number(1024~65535):\n");
        scanf("%s",port);
        if(strlen(port)<6){
            key = atoi(port);
            if(key>=1024 && key <= 65535){
                key = 0;
                continue;
            }
            key = 1;
        }
        printf("port number must be 1024 ~ 65535\n");
    }
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(port));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);


	int listenfd;
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket error:");
		exit(0);
	}
	if(bind(listenfd,(struct sockaddr *)&addr, sizeof(struct sockaddr))< 0 ){perror("bind:"); exit(0);}
	pthread_t pt;
	pthread_create(&pt,NULL,wait_for_pay,(void*)listenfd);

    char buf[1000];
    sprintf(buf,"%s#%s\n",input,port);
	
    if(write(sockfd,buf,strlen(buf)) < 0){
        perror("Error while sending to server:");
        exit(0);
    }
	
	int read_len;

    if((read_len = read(sockfd,buf,1000)) <= 0){perror("Connection abort:");exit(0);}
    
	printf("readlen %d\n",read_len);
	
    if(strstr(buf,"220 AUTH_FAIL") == NULL)
    {
		handle_list(buf,sockfd,read_len);
        return 0;
    }
	else
	{
		pthread_cancel(pt);
		close(listenfd);
		printf("No this user\n\n");
		return 1;
	}

}
void handle_list(char *buf,int sockfd,int read_len){
    char *balance = strtok(buf,"\n");
	int tmp = strlen(balance) + 1;
    printf("Your balance: %s\n",balance);
	if(tmp == read_len){
		if((read_len = read(sockfd,buf,1000)) <= 0){perror("Connection abort:");exit(0);}
		tmp = 0;
	}
	
    char *ac = strtok(buf+tmp,"\n");
	tmp += strlen(ac) + 1;
    int ac_num = atoi(ac+27);
    int i;
    char *user,*userip,*userport;
    printf("%d user(s) online\n",ac_num);
    for(i=0;i<ac_num;i++){
		if(tmp == read_len){
			if((read_len = read(sockfd,buf,1000)) <= 0){perror("Connection abort:");exit(0);}
			tmp = 0;
		}
        user = strtok(buf + tmp,"#");
		tmp += strlen(user)+1;
        
        userip = strtok(NULL,"#");
		tmp += strlen(userip) + 1;

        userport = strtok(NULL,"\n");
		tmp += strlen(userport) + 1;

        printf("User %d\nUser name : %s\nUser IP : %s\nUser port : %s\n\n",i,user,userip,userport);
	}

}

int bye(int sockfd){
	char buf[500];
	sprintf(buf,"Exit\n");

	if(write(sockfd,buf,strlen(buf)) <= 0){perror("error while writing to server:");exit(0);}
    if(read(sockfd,buf,1000) <= 0){perror("Connection abort:");exit(0);}
	
	if(strstr(buf,"Bye")!= NULL)
		exit(0);
	else{
		printf("error get %s from server while exit\n",buf);
		exit(0);
	}

}
