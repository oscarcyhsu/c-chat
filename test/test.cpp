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
   char str[5] = "#123";
   char* p = strtok(str, "#");
   printf("%s\n", p);

}