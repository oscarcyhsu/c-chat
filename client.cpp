#include <stdio.h>
#include <unistd.h>
#include <vector>
#include <stdlib.h>
#include <vector>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <signal.h>
#include <assert.h>
#include <fcntl.h>

using namespace std;

typedef struct
{
	int online;
	int port;
	char host[50];
	char name[100];

} client;

typedef struct{
   
   
   int file_size;
   char user[50],name[100],*content;
   
}file;

typedef struct{
	int fd;
	char host[50];
}conn;


pthread_mutex_t files_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m_lock = PTHREAD_MUTEX_INITIALIZER;

vector<file> files;
vector<client> clis;
pthread_mutex_t cli_lock = PTHREAD_MUTEX_INITIALIZER;
int socket_close_on_INT;
int listenport;
char my_username[50];

int reg(int sockfd);
int sign(int sockfd);
int transfer(int sockfd);
int bye(int sockfd);
int save_file();
int popfile();
int transfer();
void* BackTran(void *f);
void handle_list(char *buf, int sockfd, int read_len);
void handle_offline_msg(char *buf, int sockfd, int read_len);
int find_user(char *name);

int send_msg(int sockfd, char username[], char content[]);
int dump_history(int sockfd, char* withwho);

void SIGINT_handler(int signo){
   bye(socket_close_on_INT);
   exit(-1);
}
void *wait_for_pay(void *listenfd);
void *deal_with_client(void *con);

int main(int argc, char **argv)
{
   if (argc != 3)
   {
      printf("Error, enter [ip] [port]\n");
      exit(0);
   }

   printf("Conneting with the server, please wait.\n");
   int sockfd;
   int enable = 1;
   struct sockaddr_in addr;
   char* p_from, *p_to, *p_content;

   addr.sin_family = AF_INET;
   addr.sin_port = htons(atoi(argv[2]));
   addr.sin_addr.s_addr = inet_addr(argv[1]);
   if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      perror("socket error:");
      exit(0);
   }
   if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
   {
      perror("setsockopt(SO_REUSEADDR) failed");
      exit(1);
   }
   if (connect(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0)
   {
      perror("Connetion error:");
      exit(0);
   }
   int key = 1;
   char input[100], buf[1000];
   if (read(sockfd, buf, 1000) < 0)
   {
      perror("connetion error:");
   }

   socket_close_on_INT = sockfd;
   signal(SIGINT, SIGINT_handler);
   while (key)
   {
      printf("enter the number (0:register,1:sign in):\n");
      scanf("%s", input);
      if (strlen(input) != 1 || (input[0] != '0' && input[0] != '1'))
      {
         printf("enter the number in the right way(0:register, 1:sign in)!!!\n");
      }
      else if (input[0] == '0')
         key = reg(sockfd);
      else if (input[0] == '1')
         key = sign(sockfd);
   }
   printf("Enter (Q: quit, U: update the list, M: message):\n");
   fd_set reading_set, workingR_set;
   FD_ZERO(&reading_set);
   FD_SET(STDIN_FILENO, &reading_set);
   FD_SET(sockfd, &reading_set);
   
   while(1){
      memcpy(&workingR_set, &reading_set, sizeof(reading_set));
      select(128, &workingR_set, NULL, NULL, NULL);
      int read_len;
      for(int i = 0; i <= 128; i++){
         if(FD_ISSET(i, &workingR_set) && i == STDIN_FILENO){
            printf("fd: %d is ready to be read\n", i);
            if((read_len = read(STDIN_FILENO, input, sizeof(input))) < 0){
               perror("read input");
               exit(-1);
            }
            input[read_len - 1] = '\0'; // newline -> \0
            printf("input:<%s>\n", input);
            if (input[0] == 'Q')
               bye(sockfd);
            else if (input[0] == 'U'|| input[0] == 'T'){
               sprintf(buf, "List\n");
               if (write(sockfd, buf, strlen(buf)) <= 0)
               {
                  perror("error while writing to server:");
                  exit(0);
               }
               if ((read_len = read(sockfd, buf, 1000)) <= 0)
               {
                  perror("Connection abort:");
                  exit(0);
               }
               //printf("read:<%s>, readlen:<%d>", buf, read_len);
               handle_list(buf, sockfd, read_len);
               if(input[0] == 'T')
                  transfer();
            }
            else if( input[0] == 'M'){
               //send to user
               char username[50], content[1000];
               char* p;
               strtok(input,"-");
               p = strtok(NULL,"-");
               printf("p1:%s\n", p);
               if (p == NULL){ printf("format: M-username-conetent\n"); continue;}
               else{ sprintf(username ,"%s\0", p);}

               p = strtok(NULL, "-");
               printf("p2:%s\n", p);
               if (p == NULL){ printf("format: M-username-conetent\n"); continue;}
               else{ sprintf(content, "%s\0", p);}
               send_msg(sockfd, username, content);
            }
            else if( input[0] == 'D'){ //dump history
               strtok(input, "-");
               p_to = strtok(NULL, "-");
               printf("dump history with <%s>\n", p_to);
               dump_history(sockfd, p_to);
            }
            printf("Enter (Q: quit, U: update the list, M: message):\n");
         }

         else if(FD_ISSET(i, &workingR_set) && i == sockfd){ // async message
            printf("fd: %d is ready to be read\n", i);
            if((read_len = read(sockfd, buf, sizeof(buf))) < 0){
               perror("read async message");
               exit(-1);
            }
            printf("sockfd buf:<%s>\n", buf);
            strtok(buf, "\n");
            assert(buf[0] == 'A'); // check async message
            printf("new message!\n");
            strtok(buf, "#");
            p_from = strtok(NULL, "#");
            p_content = strtok(NULL, "#");
            printf("<%s>:%s\n", p_from, p_content);
         }
      }
   }

   // while (scanf("%s", input))
   // {
      
   // }
}

int reg(int sockfd)
{
   char input[50];
   int key = 1;

   while (key)
   {
      printf("Enter the name you want to register(2~20 number or alphebat):\n");
      scanf("%s", input);
      int i, len = strlen(input);
      if (len >= 2 && len <= 50)
      {
         key = 0;
         for (i = 0; i < len; i++)
         {
            if (!(input[i] >= '0' && input[i] <= '9' ||
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
   sprintf(buf, "REGISTER#%s\n", input);

   if (write(sockfd, buf, strlen(buf)) < 0)
   {
      perror("connection abort:");
      exit(0);
   }

   if (read(sockfd, buf, 100) <= 0)
   {
      perror("server abort:");
      exit(0);
   }
   if (buf[0] == '1')
   {
      printf("register success\n\n");
   }
   else
   {
      printf("register failed\n\n");
   }
   return 1;
}

int sign(int sockfd)
{

   char input[50];

   int key = 1;
   while (key)
   {
      printf("Enter your name(2~20 number or alphebat):\n");
      scanf("%s", input);
      int i, len = strlen(input);

      if (len >= 2 && len <= 50)
      {
         key = 0;
         for (i = 0; i < len; i++)
         {
            if (!(input[i] >= '0' && input[i] <= '9' ||
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
   while (key)
   {
      printf("Enter the port number(1024~65535):\n");
      scanf("%s", port);
      if (strlen(port) < 6)
      {
         key = atoi(port);
         if (key >= 1024 && key <= 65535)
         {
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

   listenport = atoi(port);
   int listenfd;
   if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
   {
      perror("socket error:");
      exit(0);
   }
   if (bind(listenfd, (struct sockaddr *)&addr, sizeof(struct sockaddr)) < 0)
   {
      perror("bind:");
      exit(0);
   }
   pthread_t pt;
   pthread_create(&pt, NULL, wait_for_pay, (void *)listenfd);

   char buf[1000];
   sprintf(buf, "S#%s#%s\n", input, port);
   if (write(sockfd, buf, strlen(buf)) < 0)
   {
      perror("Error while sending to server:");
      exit(0);
   }

   strcpy(my_username, input);
   int read_len;

   if ((read_len = read(sockfd, buf, 1000)) <= 0)
   {
      perror("Connection abort:");
      exit(0);
   }

   //printf("readlen %d\n", read_len);

   if (strstr(buf, "220 AUTH_FAIL") == NULL)
   {
      handle_list(buf, sockfd, read_len);
      if ((read_len = read(sockfd, buf, 1000)) <= 0){perror("Connection abort:"); exit(0);}
      if(strstr(buf, "you have offline msg\n") != NULL){
         printf("you have offline msg\n");
         handle_offline_msg(buf, sockfd, read_len);
      }
      else{
         printf("no new msg\n");
      }
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
void handle_list(char *buf, int sockfd, int read_len)
{

	int tmp = 0;
	clis.clear();

	char *ac = strtok(buf + tmp, "\n");
	tmp += strlen(ac) + 1; // let tmp = readlen?
	int ac_num = atoi(ac + 27);
	int i;
	char *user, *userip, *userport;
	printf("%d user(s) in system\n", ac_num);
	for (i = 0; i < ac_num; i++)
	{
		client cli;
      //printf("tmp/readlen:%d/%d\n", tmp, read_len);
		if (tmp == read_len)
		{
         if ((read_len = read(sockfd, buf, 1000)) <= 0)
         {
            perror("Connection abort:");
            exit(0);
         }
         tmp = 0;
         //printf("buf:%s\n", buf);
		}

		user = strtok(buf + tmp, "#");
		strcpy(cli.name, user);
		tmp += strlen(user) + 1;
		// printf("user %s\n", cli.name);

		int online = atoi(strtok(NULL, "#"));
		cli.online = online;
		tmp += 2;
		// printf("online %d\n", cli.online);

		userip = strtok(NULL, "#");
		tmp += strlen(userip) + 1;
		strcpy(cli.host, userip);
		// printf("ip %s\n", cli.host);

		userport = strtok(NULL, "\n");
		tmp += strlen(userport) + 1;
		cli.port = atoi(userport);
		clis.push_back(cli);
		// printf("port %d\n", cli.port);

		printf("User %d\nUser name : %s\nUser IP : %s\nUser port : %d\n\n", clis.size() - 1, clis[i].name, clis[i].host, clis[i].port);
	}
}

void handle_offline_msg(char *buf, int sockfd, int read_len){
   int tmp = 0;
   char* p_from, *p_content;
   //printf("handling<%s>\n", buf);
   strtok(buf, "\n");
   tmp += strlen(buf)+1;

   while(1){
      if (tmp == read_len)
		{
         if ((read_len = read(sockfd, buf, 1000)) <= 0)
         {
            perror("Connection abort:");
            exit(0);
         }
         tmp = 0;
         //printf("buf:%s\n", buf);
      }

      p_from = strtok(buf + tmp, "#");
      //printf("p_from:<%s>\n", p_from);
		tmp += strlen(p_from) + 1;
      if(p_from[0] == '$'){
         break;
      }
		p_content = strtok(NULL, "$");
      //printf("p_content:<%s>\n", p_content);
		tmp += strlen(p_content) + 1;
		printf("<%s>:%s\n", p_from, p_content);
   }
}

int bye(int sockfd)
{
   char buf[500];
   sprintf(buf, "Exit\n");

   if (write(sockfd, buf, strlen(buf)) <= 0)
   {
      perror("error while writing to server:");
      exit(0);
   }
   if (read(sockfd, buf, 1000) <= 0)
   {
      perror("Connection abort:");
      exit(0);
   }

   if (strstr(buf, "Bye") != NULL)
      exit(0);
   else
   {
      printf("error get %s from server while exit\n", buf);
      exit(0);
   }
}

int send_msg(int sockfd, char username[], char content[]){
   char buf[1100];
   sprintf(buf, "M#%s#%s$", username, content);
   if (write(sockfd, buf, strlen(buf)) <= 0)
   {
      perror("error while writing to server:");
      exit(0);
   }
   if (read(sockfd, buf, sizeof(buf)) <= 0)
   {
      perror("Connection abort:");
      exit(0);
   }
   return 1;
}

void* wait_for_pay(void *listenfd){
    long long lifd = (long long)listenfd;
    if(listen((int)lifd,10) < 0)
        perror("listen: ");
    int newfd;

	struct sockaddr_in ac_addr;
	ac_addr.sin_family = AF_INET;
	ac_addr.sin_port = htons(listenport);
	ac_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	int ac_size = sizeof(ac_addr);
	//boost::threadpool::pool thread_pool(thread_num);
	while(1){
		
		if((newfd = accept(lifd,(struct sockaddr*)&ac_addr,(socklen_t *) &ac_size)) < 0 ){
			perror("accept ");
			exit(1);
		}
		conn *con =(conn*) malloc(sizeof(conn));
		con->fd = newfd;
		strcpy(con->host,inet_ntoa(ac_addr.sin_addr));
		printf("new connection from %s\n",con->host);
		pthread_t pnum;
		//thread_pool.schedule(boost::bind(deal_with_client,(void *)con));
		pthread_create(&pnum,NULL,&deal_with_client,(void *)con);
		pthread_detach(pnum);

	}


}
void* deal_with_client(void *con){
	int fd = ((conn *) con) -> fd;
	char host[50];
	strcpy(host,((conn *) con) -> host);
   printf("con\n");
	free(con);
	

	char buf[2048];

	
	//accept
	sprintf(buf,"connection accepted\n");
   file f1;
	if(write(fd,buf,strlen(buf))<0){perror("write"); return 0;}
   int name_siz,file_siz;
   if(read(fd,&name_siz,sizeof(int))< 0){perror("read name size"); return 0;}
  
   printf("name size :%d\n",name_siz);

   if(read(fd,f1.name,name_siz)<0){perror("read file name"); return 0;}
   f1.name[name_siz] = '\0';
   
   

   if(read(fd,&f1.file_size,sizeof(int))<0){perror("read file size "); return 0;}
   
   pthread_mutex_lock(&m_lock);
   f1.content = (char*) malloc(file_siz);
   pthread_mutex_unlock(&m_lock);
   
   int tmp = 0;
   while(tmp < f1.file_size){
      int len;
      if( (len = read(fd, f1.content + tmp, f1.file_size-tmp)) < 0){perror("reading file"); return 0;}
      tmp += len;
   }
   int len = read(fd,buf,100);
   if(len < 0){perror("read user name error"); return 0;}
   


   for(int i;i<len;i++){f1.user[i] = buf[i];}
   f1.user[len] = '\0';

   pthread_mutex_lock(&files_lock);
   files.push_back(f1);
   pthread_mutex_unlock(&files_lock);
   
   write(fd,"end",3);
   printf("recv file from %s, please check by instruction \n",f1.user);  


	close(fd);
   return 0;
}

int transfer(){
   
   int file_fd;
   char file_name[100];
   int file_size;
   printf("Enter the filename with path:\n");
   scanf("%s",file_name);
   struct stat st;
   if (stat(file_name, &st) < 0){
      perror("Invalid file");
      return -1;
   }
   file_size = st.st_size;
   pthread_mutex_lock(&m_lock);
   file *f1 = (file*) malloc(sizeof(file));
   pthread_mutex_unlock(&m_lock);

   f1->file_size = file_size;
   strcpy(f1->name,file_name);

   

   printf("enter the user you want to transfer\n");
   scanf("%s",f1->user);

   pthread_t pnum;

   pthread_create(&pnum,NULL,&BackTran,(void *)f1);
   pthread_detach(pnum);

   return 0;  

}

void* BackTran(void *f){
   int sockfd;
   file f1;
   memcpy(&f1,f,sizeof(file));
   pthread_mutex_lock(&m_lock);
   printf("backtran\n");
   free(f);
   pthread_mutex_unlock(&m_lock);
   pthread_mutex_lock(&cli_lock);
   int user_num = find_user(f1.user);
   printf("user_num %d\n",user_num);
   struct sockaddr_in addr;
   if(user_num!= -1){
      if(clis[user_num].online)
      {
         printf("Conneting with the user, please wait.\n");

         addr.sin_family = AF_INET;
         addr.sin_port = htons(clis[user_num].port);
         addr.sin_addr.s_addr = inet_addr(clis[user_num].host);
         
         if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
         {
            perror("socket error:");
            exit(0);
         }
      }
      else{
         user_num = -1;
      }
   }
      
   pthread_mutex_unlock(&cli_lock);
   printf("user num after %d\n",user_num);
   if (user_num == -1){
      printf("No the user: %s online.",f1.user);

      return (void*) -1;
   }
   printf("connection starting\n");
   if(connect(sockfd,(struct sockaddr *)&addr,sizeof(struct sockaddr)) < 0)
   {
      perror("Connetion error:");
      printf("Maybe the user is offline or abort.\n");

      return (void*) -1;
   }
   char buf[100];
   if(read(sockfd,buf,100)<0){perror("Not accept"); return 0;}
   pthread_mutex_lock(&m_lock);
   char *content = (char*) malloc(f1.file_size);
   pthread_mutex_unlock(&m_lock);
   int filefd = open(f1.name, O_RDONLY);
   if(filefd< 0){perror("open error"); printf("check if the file exist\n"); return 0;   }
   
   if(read(filefd,content,f1.file_size)<0){perror("read file"); return 0;}
   close(filefd);

   int namelen= strlen(f1.name);
   if(write(sockfd,&namelen,sizeof(int))< 0){perror("write fname len");return 0;}
   if(write(sockfd,f1.name,strlen(f1.name)) < 0){perror("write fname"); return 0;}
   if(write(sockfd,&f1.file_size,sizeof(int)) < 0) {perror("write file size"); return 0;}
   
   int tmp = 0;
   int len;
   while(tmp < f1.file_size){
      len = write(sockfd, content + tmp, f1.file_size-tmp);
      if(len < 0){perror("error while transfering file"); return 0;}
      tmp += len;
   }

   if(write(sockfd,my_username,strlen(my_username))<0){perror("write name;"); return 0;}

   if(read(sockfd,buf,10)<0){perror("reading end"); return 0;}
   printf("finish file transfer\n");
   close(sockfd);

   
}

int save_file(){
   pthread_mutex_lock(&files_lock);
   char name[100];
   int i = 0;
   for(int i =0;i<files.size();i++)
   {
      printf("file from %s, name: %s, size: %d\n Save? [Y/N]\n",files[i].user,files[i].name,files[i].file_size);
      char c;
      scanf("%c",&c);
      if(c == 'Y'){
         printf("enter the file name to save\n");
         scanf("%s",name);
         int fd = open(name,O_WRONLY|O_CREAT|O_TRUNC);
         if(write(fd,files[i].content,files[i].file_size)< 0 ){perror("error while writing the file");}
      }
   }
   while(!files.empty()){
      popfile();
   }
   pthread_mutex_unlock(&files_lock);
}
int find_user(char *name){
   for(int i=0;i<clis.size();i++){
      if(strcmp(clis[i].name,name) == 0){
         return i;
      }
   }
   return -1;
}

int popfile(){

   pthread_mutex_lock(&m_lock);
   free(files[files.size()-1].content);
   pthread_mutex_unlock(&m_lock);
   files.pop_back();
}

int dump_history(int sockfd, char* withwho){
   char buf[1100], read_len;
   char *p_who, *p_from, *p_content;

   int tmp = 0, readlen = 0;

   sprintf(buf, "D#%s\n", withwho);
   if (write(sockfd, buf, strlen(buf)) <= 0)
   {
      perror("error while writing to server:");
      exit(0);
   }
   // todo parse return history
   while(1){
      if (tmp == read_len)
		{
         if ((read_len = read(sockfd, buf, 1000)) <= 0)
         {
            perror("Connection abort:");
            exit(0);
         }
         tmp = 0;
         //printf("buf:%s\n", buf);
      }

      p_from = strtok(buf + tmp, "#");
      //printf("p_from:<%s>\n", p_from);
		tmp += strlen(p_from) + 1;
      if(p_from[0] == '$'){
         printf("end of history\n");
         return 0;
      }
		p_content = strtok(NULL, "$");
      //printf("p_content:<%s>\n", p_content);
		tmp += strlen(p_content) + 1;
      if(strcmp(my_username, p_from) == 0)
		   printf("<%s>\n%s\n\n", p_from, p_content);
      else{
         int len;
         len = strlen(p_from)+2;
         for(int j = 0; j < 20-len;j++)
            printf(" ");
         printf("<%s>\n", p_from);

         len = strlen(p_content);
         for(int j = 0; j < 20-len;j++)
            printf(" ");
         printf("%s\n\n", p_content);
      }
   }
}

