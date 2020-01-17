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
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <vector>

using namespace std;

int main(int argc, char **argv){
   int fd;
   int a = 5, b = 10;
   int c = 0, d = 0;
   fd = open("log", O_RDWR | O_CREAT, 0700);
   if(fd < 0){
      perror("open");
      exit(-1);
   }
   pwrite(fd, &a, sizeof(int), 0);
   pwrite(fd, &b, sizeof(int), 16);

   pread(fd, &c, sizeof(int), 0);
   pread(fd, &d, sizeof(int), 16);
   printf("%d %d\n", c, d);

}