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

using namespace std;

typedef struct
{
	int online;
	int port;
	char host[50];
	char name[100];

} client;

vector<client> clis;
pthread_mutex_t cli_lock = PTHREAD_MUTEX_INITIALIZER;
int socket_close_on_INT;
char my_username[50];

int reg(int sockfd);
int sign(int sockfd);
int bye(int sockfd);
void handle_list(char *buf, int sockfd, int read_len);
void handle_offline_msg(char *buf, int sockfd, int read_len);

int send_msg(int sockfd, char username[], char content[]);
int dump_history(int sockfd, char* withwho);

void SIGINT_handler(int signo){
   bye(socket_close_on_INT);
   exit(-1);
}
void *wait_for_pay(void *listenfd)
{
   long long lifd = (long long)listenfd;
   if (listen((int)lifd, 10) < 0)
      perror("listen: ");
}

int main(int argc, char **argv)
{
   if (argc != 3)
   {
      printf("Error, enter [ip] [port]\n");
      exit(0);
   }

   printf("Conneting with the server, please wait.\n");
   int sockfd;

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
            else if (input[0] == 'U'){
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