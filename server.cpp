#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <vector>
#include <signal.h>
#include <assert.h>
#include <openssl/sha.h>

using namespace std;

#define L_ONLINE 1
#define L_OFFLINE 0
typedef struct
{
   char name[50];
} Array;

typedef struct
{
   int online, balance;
   char port[7];
   char host[50];
   char name[100];
} client;
typedef struct
{
   int fd;
   char host[50];
} conn;
typedef struct{
   char from[50];
   char to[50];
   char content[1000];
} msg;

int online_num = 0;
vector<client> clis; //clients
vector<int>  locked_log;

pthread_mutex_t m_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t tok_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t f_lock = PTHREAD_MUTEX_INITIALIZER;


int set_lock(int file_fd, int type)
{
   if (pthread_mutex_lock(&f_lock) < 0) { perror("mutex_lock"); return -1; }
   if(type == F_ULOCK){
      for(int j = 0; j < locked_log.size(); j++){
         if(locked_log[j] == file_fd){
            locked_log.erase(locked_log.begin() + j);
         }
      }
   }
   else if(type == F_WRLCK || type == F_RDLCK){
      locked_log.push_back(file_fd);
   }
   if (pthread_mutex_unlock(&f_lock) < 0) { perror("mutex_lock"); return -1; }
   return 0;
}

void *deal_with_client(void *cli);
void set_log(int logfile, msg message, int online);

int main(int argc, char **argv)
{
   int enable = 1;
   struct stat st = {0};
   if (argc != 3)
   {
      printf("please enter: ./server [port][thread_num]");
      exit(1);
   }

   if (stat("log", &st) != 0)
   {
      if (mkdir("log", 0700) < 0)
      {
         perror("create log folder");
         exit(-1);
      }
   }

   int listenfd, port;
   int thread_num = atoi(argv[2]);
   listenfd = socket(AF_INET, SOCK_STREAM, 0);
   if (listenfd < 0)
   {
      perror("socket()");
      exit(1);
   }
   if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
   {
      perror("setsockopt(SO_REUSEADDR) failed");
      exit(1);
   }
   port = atoi(argv[1]);
   if (port < 1024 || port > 65535)
   {
      printf("port must between 1024 ~ 65535");
      exit(1);
   }
   //addr sets
   struct sockaddr_in addr;
   addr.sin_family = AF_INET;
   addr.sin_port = htons(port);
   addr.sin_addr.s_addr = htonl(INADDR_ANY);
   if (bind(listenfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
   {
      perror("bind: ");
      exit(1);
   }

   if (listen(listenfd, 5) < 0)
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

   while (1)
   {
      if ((newfd = accept(listenfd, (struct sockaddr *)&ac_addr, (socklen_t *)&ac_size)) < 0)
      {
         perror("accept ");
         exit(1);
      }
      conn *con = (conn *)malloc(sizeof(conn));
      con->fd = newfd;
      strcpy(con->host, inet_ntoa(ac_addr.sin_addr));
      printf("new connection %d, %s\n", newfd, con->host);
      pthread_t pnum;
      pthread_create(&pnum, NULL, &deal_with_client, (void *)con);
      pthread_detach(pnum);
   }
}

void *deal_with_client(void *con)
{
   int fd = ((conn *)con)->fd;
   int fifo_r, fifo_w, logfile;
   int read_len;
   int num_msg, read_msg;
   char host[50], path[100];
   fd_set workingR_set, reading_set;
   client my_client;
   msg message;
   FD_ZERO(&reading_set);
   FD_SET(fd, &reading_set);

   strcpy(host, ((conn *)con)->host);
   free(con);

   char buf[1000];

   //accept
   sprintf(buf, "connection accepted\n");
   if (write(fd, buf, strlen(buf)) < 0)
   {
      perror("write");
      return 0;
   }
   //register or sign
   int i, j, key, user_num = -1;
   char *name;

   while (user_num == -1)
   {
      int l;
      if ((l = read(fd, buf, sizeof(buf)) <= 0))
      {
         perror("read");
         return 0;
      }
      printf("r or s, buf:<%s>\n", buf);
      //register
      if (buf[0] == 'R')
      {
         key = 1;
         if (pthread_mutex_lock(&tok_lock) < 0) { perror("mutex_lock"); return 0; }
         name = strtok(buf, "#");
         name = strtok(NULL, "\n");
         if (pthread_mutex_unlock(&tok_lock) < 0) { perror("mutex_unlock"); return 0; }

         //lock mutex for check
         if (pthread_mutex_lock(&m_lock) < 0)
         {
            perror("mutex_lock");
            return 0;
         }
         for (i = 0; i < clis.size(); i++)
         {
            if (strcmp(name, clis[i].name) == 0)
            {
               key = 0;
               break;
            }
         }
         if (key)
         {
            //set new client
            client cli;
            cli.online = 0;
            cli.balance = 1000;
            strcpy(cli.name, name);

            clis.push_back(cli);

            struct stat st;
            char path[100];
            sprintf(path, "log/%s", cli.name);
            if (stat(path, &st) != 0)
            {
               if (mkdir(path, 0700) < 0)
               {
                  perror("create personal log folder");
                  exit(-1);
               }
            }
            sprintf(path, "log/%s/fifo", cli.name);
            if (stat(path, &st) != 0)
            {
               if (mkfifo(path, 0700) < 0)
               {
                  perror("create fifo");
                  exit(-1);
               }
            }

            sprintf(path, "log/%s/logfile", cli.name);
            if (stat(path, &st) != 0)
            {
               if ((logfile = open(path, O_CREAT | O_RDWR, 0700)) < 0)
               {
                  perror("create logfile");
                  exit(-1);
               }
               num_msg = read_msg = 0;
               if (pwrite(logfile, &num_msg, sizeof(int), 0) < 0)
               {
                  perror("write log");
                  exit(-1);
               }
               if (pwrite(logfile, &read_msg, sizeof(int), sizeof(int)) < 0)
               {
                  perror("write log");
                  exit(-1);
               }
               if (close(logfile) < 0) {perror("close log"); exit(-1);}
            }
            // else{
            //    if ((logfile = open(path, O_RDWR, 0700)) < 0)
            //    {
            //       perror("open logfile");
            //       exit(-1);
            //    }
            // }

            sprintf(buf, "100 OK\n");
            if (write(fd, buf, strlen(buf)) < 0)
            {
               perror("write");
               return 0;
            }
         }
         else
         {
            sprintf(buf, "210 FAIL\n");
            if (write(fd, buf, strlen(buf)) < 0)
            {
               perror("write");
               return 0;
            }
         }
         //unlock after check and change
         if (pthread_mutex_unlock(&m_lock) < 0)
         {
            perror("mutex_unlock");
            return 0;
         }
      }
      else if (buf[0] == 'S') //sign in
      {
         //sign
         char *port;
         if (pthread_mutex_lock(&tok_lock) < 0) { perror("mutex_lock"); return 0; }
         name = strtok(buf + 1, "#");
         port = strtok(NULL, "\n");
         if (pthread_mutex_unlock(&tok_lock) < 0) { perror("mutex_unlock"); return 0; }
         strcpy(my_client.name, name);
         strcpy(my_client.port, port);
         if (pthread_mutex_lock(&m_lock) < 0)
         {
            perror("mutex_lock");
            return 0;
         }
         for (i = 0; i < clis.size(); i++)
         {
            if (strcmp(clis[i].name, name) == 0)
            {
               //exist
               sprintf(path, "log/%s/fifo", name);
               if ((fifo_r = open(path, O_RDONLY | O_NONBLOCK)) < 0)
               {
                  perror("open read fifo");
                  exit(-1);
               }
               FD_SET(fifo_r, &reading_set);
               online_num++;
               user_num = i;
               clis[i].online = 1;
               strcpy(clis[i].port, port);
               strcpy(clis[i].host, host);
               // send user list
               sprintf(buf, "number of accounts online: %d\n", online_num);
               if (write(fd, buf, strlen(buf)) < 0)
               {
                  perror("write");
                  return 0;
               }
               for (j = 0; j < clis.size(); j++)
               {
                  if (clis[j].online)
                  {
                     sprintf(buf, "%s#%d#%s#%s\n", clis[j].name, clis[j].online, clis[j].host, clis[j].port);
                     if (write(fd, buf, strlen(buf)) < 0)
                     {
                        perror("write");
                        return 0;
                     }
                  }
               }
               // send offline msg
               sprintf(path, "log/%s/logfile", my_client.name);
               if ((logfile = open(path, O_RDWR)) < 0) { perror("open logfile"); exit(-1); }
               if(set_lock(logfile, F_WRLCK) < 0) {perror("set lock"); exit(-1); }
               // retrieve unread message
               if(pread(logfile, &num_msg, sizeof(int), 0) < 0) {perror("read"); exit(-1); }
               if(pread(logfile, &read_msg, sizeof(int), sizeof(int)) < 0) {perror("read"); exit(-1); }
               if(num_msg != read_msg){
                  sprintf(buf, "you have offline msg\n");
                  if (write(fd, buf, strlen(buf)) < 0){perror("write"); exit(-1);}

                  printf("num_msg<%d>/read_msg<%d>\n", num_msg, read_msg);
                  for(int j = read_msg+1; j <= num_msg; j++){
                     pread(logfile, &message, sizeof(msg), j*sizeof(msg));
                     printf("unread, from:%s, to:%s, content:<%s>\n", message.from, message.to, message.content);
                     sprintf(buf, "%s#%s$", message.from, message.content);
                     if (write(fd, buf, strlen(buf)) < 0){ perror("write"); exit(-1);}
                  }
                  sprintf(buf, "#$"); //end of msg
                  write(fd, buf, strlen(buf));

                  if(pwrite(logfile, &num_msg, sizeof(int), sizeof(int)) < 0) {perror("write log"); exit(-1);}
                  if(pread(logfile, &read_msg, sizeof(int), sizeof(int)) < 0) {perror("read"); exit(-1); }
                  printf("after handle unread, num_msg<%d>/read_msg<%d>\n", num_msg, read_msg);
               }
               else{
                  sprintf(buf, "no new msg\n");
                  if (write(fd, buf, strlen(buf)) < 0){perror("write"); exit(-1);}
               }
               if(set_lock(logfile, F_UNLCK) < 0) {perror("unlock"); exit(-1); }
               if(close(logfile)<0) {perror("close logfile"); exit(-1); }

               break;
            }
         }
         if (pthread_mutex_unlock(&m_lock) < 0)
         {
            perror("mutex_unlock");
            return 0;
         }
         if (user_num == -1)
         {
            sprintf(buf, "220 AUTH_FAIL\n");
            if (write(fd, buf, strlen(buf)) < 0)
            {
               perror("write");
               return 0;
            }
         }
      }
   }
   while (1)
   {
      // select on packet from client and FIFO(for inter-thread communication)
      memcpy(&workingR_set, &reading_set, sizeof(reading_set));
      select(128, &workingR_set, NULL, NULL, NULL);
      for (int i = 0; i <= 128; i++)
      {
         if (FD_ISSET(i, &workingR_set) && i == fd)
         {
            printf("fd: %d is ready to be read\n", i);
            if (read(fd, buf, sizeof(buf)) < 0)
            {
               perror("read");
               return 0;
            }
            if (buf[0] == 'L') //online userlist
            {
               if (pthread_mutex_lock(&m_lock) < 0)
               {
                  perror("mutex_lock");
                  return 0;
               }

               sprintf(buf, "number of accounts insys : %d\n", clis.size());
               if (write(fd, buf, strlen(buf)) < 0)
               {
                  perror("write");
                  return 0;
               }
               for (j = 0; j < clis.size(); j++)
               {
                  if (1)
                  {
                     sprintf(buf, "%s#%d#%s#%s\n", clis[j].name, clis[j].online, clis[j].host, clis[j].port);
                     if (write(fd, buf, strlen(buf)) < 0)
                     {
                        perror("write");
                        return 0;
                     }
                  }
               }
               if (pthread_mutex_unlock(&m_lock) < 0)
               {
                  perror("mutex_unlock");
                  return 0;
               }
            }
            else if (buf[0] == 'E')
            {
               //log out
               sprintf(buf, "Bye\n");
               if (pthread_mutex_lock(&m_lock) < 0)
               {
                  perror("mutex_lock");
                  return 0;
               }
               online_num--;
               clis[user_num].online = 0;
               if (pthread_mutex_unlock(&m_lock) < 0)
               {
                  perror("mutex_unlock");
                  return 0;
               }
               if (write(fd, buf, strlen(buf)) < 0)
               {
                  perror("write");
                  return 0;
               }
               close(fd);
               FD_CLR(fd, &reading_set);
               close(fifo_r);
               FD_CLR(fifo_r, &reading_set);
               return 0;
            }
            else if (buf[0] == 'M')
            {
               if (pthread_mutex_lock(&tok_lock) < 0) { perror("mutex_lock"); return 0; }
               strtok(buf, "$"); // use $ as end, because content may have newline characters
               printf("message, buf<%s>\n", buf);

               char *p_to_username, *p_content;
               strtok(buf, "#");

               p_to_username = strtok(NULL, "#");
               p_content = strtok(NULL, "#");
               if (pthread_mutex_unlock(&tok_lock) < 0) { perror("mutex_unlock"); return 0; }
               assert(strcmp(my_client.name, p_to_username) != 0);

               // todo: add log
               // add sender's log
               strcpy(message.from, my_client.name);
               strcpy(message.to, p_to_username);
               strcpy(message.content, p_content);

               sprintf(path, "log/%s/logfile", message.from);
               if ((logfile = open(path, O_RDWR)) < 0) { perror("open logfile"); exit(-1); }
               set_log(logfile, message, L_ONLINE);
               if(close(logfile)<0) {perror("close logfile"); exit(-1); }

               // notify receiver's serving thread if receiver is online
               if (pthread_mutex_lock(&m_lock) < 0)
               {
                  perror("mutex_lock");
                  return 0;
               }
               for (j = 0; j < clis.size(); j++)
               {
                  printf("matching name:%s/%s\n", p_to_username, clis[j].name);
                  if (strcmp(p_to_username, clis[j].name) == 0)
                  {
                     if(clis[j].online){
                        printf("%s is online\n", clis[j].name);
                        sprintf(path, "log/%s/fifo", message.to);
                        printf("open path:<%s>\n", path);
                        if ((fifo_w = open(path, O_WRONLY)) < 0)
                        {
                           perror("open write fifo error");
                           exit(-1);
                        }

                        sprintf(buf, "Async#%s#%s\n", message.from, message.content);
                        if (write(fifo_w, buf, strlen(buf)) < 0)
                        {
                           perror("write");
                           exit(-1);
                        }
                        if (close(fifo_w) < 0){
                           perror("close");
                           exit(-1);
                        }
                        sprintf(path, "log/%s/logfile", message.to);
                        if ((logfile = open(path, O_RDWR)) < 0) { perror("open logfile"); exit(-1); }
                        set_log(logfile, message, L_ONLINE);
                        if(close(logfile)<0) {perror("close logfile"); exit(-1); }
                     }
                     else{
                        printf("%s is offline\n", clis[j].name);
                        sprintf(path, "log/%s/logfile", message.to);
                        if ((logfile = open(path, O_RDWR)) < 0) { perror("open logfile"); exit(-1); }
                        set_log(logfile, message, L_OFFLINE);
                        if(close(logfile)<0) {perror("close logfile"); exit(-1); }
                     }
                     break;
                  }
                  
               }
               if (pthread_mutex_unlock(&m_lock) < 0)
               {
                  perror("mutex_unlock");
                  return 0;
               }

               sprintf(buf, "server receives message\n");
               if (write(fd, buf, strlen(buf)) < 0)
               {
                  perror("write");
                  exit(-1);
               }
            }
            else if (buf[0] == 'D'){
               // todo D-type
               char *p, chat_with[50];
               if (pthread_mutex_lock(&tok_lock) < 0) { perror("mutex_lock"); return 0; }
               strtok(buf, "\n");
               strtok(buf, "#");
               p = strtok(NULL, "#");
               strcpy(chat_with, p);
               printf("chat with<%s>\n", chat_with);
               sprintf(path, "log/%s/logfile", my_client.name);
               printf("dump logfile:<%s>\n", path);
               if((logfile = open(path, O_RDONLY)) < 0){perror("open logfile"); return 0;}
               if(set_lock(logfile, F_RDLCK) < 0){perror("lock logfile"); return 0;}
               
               pread(logfile, &num_msg, sizeof(int), 0);
               printf("num_msg:%d\n", num_msg);
               for(int j = 1; j <= num_msg; j++){
                  pread(logfile, &message, sizeof(msg), j*sizeof(msg));
                  printf("message:from/p_with<%s/%s>, content<%s>\n", message.from, chat_with, message.content);
                  if(strcmp(message.from, chat_with) == 0 || strcmp(message.to, chat_with) == 0){
                     printf("message match:from<%s>, content<%s>\n", message.from, message.content);
                     sprintf(buf, "%s#%s$", message.from, message.content);
                     if(write(fd, buf, strlen(buf)) < 0){perror("write"); return 0;}
                  }
               }
               sprintf(buf, "#$");
               if(write(fd, buf, strlen(buf)) < 0){perror("write"); return 0;}

               if(set_lock(logfile, F_UNLCK) < 0){perror("unlock logfile"); return 0;}
               if(close(logfile) < 0){perror("close logfile"); return 0;}

               if (pthread_mutex_unlock(&tok_lock) < 0) { perror("mutex_unlock"); return 0; }
            }
         }
         else if (FD_ISSET(i, &workingR_set) && i == fifo_r)
         {
            char *p;
            read_len = read(fifo_r, buf, sizeof(buf));
            if (read_len < 0)
            {
               perror("read fifo");
               exit(-1);
            }
            else if(read_len > 0){ // close will make fifo readable but read return 0
               printf("fifo_r: %d is ready to be read\n", i);
               if (pthread_mutex_lock(&tok_lock) < 0) { perror("mutex_lock"); return 0; }
               strtok(buf, "\n");
               if (pthread_mutex_unlock(&tok_lock) < 0) { perror("mutex_unlock"); return 0; }
               printf("read from fifo:<%s>\n", buf);
               buf[strlen(buf)+1] = '\0';
               buf[strlen(buf)] = '\n';
               if (write(fd, buf, strlen(buf)) < 0)
               {
                  perror("send Async message");
                  exit(-1);
               }
            }
         }
      }
   }
   return 0;
}

void set_log(int logfile, msg message, int online){
   int num_msg, read_msg;
   if (set_lock(logfile, F_WRLCK) < 0){ // get first entry of file to get access
      perror("setlock");
      exit(-1);
   }

   if (pread(logfile, &num_msg, sizeof(int), 0) < 0)
   {
      perror("read log");
      exit(-1);
   }
   num_msg++;
   if (pwrite(logfile, &num_msg, sizeof(int), 0) < 0){
      perror("write log");
      exit(-1);
   }
   if (pwrite(logfile, &message, sizeof(msg), num_msg*sizeof(msg)) < 0){
      perror("write log");
      exit(-1);
   }

   if(online == L_ONLINE){
      if (pread(logfile, &read_msg, sizeof(int), sizeof(int)) < 0)
      {
         perror("read log");
         exit(-1);
      }
      read_msg++;
      if (pwrite(logfile, &read_msg, sizeof(int), sizeof(int)) < 0){
         perror("write log");
         exit(-1);
      }
   }

   if (set_lock(logfile, F_UNLCK) < 0){
      perror("freelock");
      exit(-1);
   }
}
