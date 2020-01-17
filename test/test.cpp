#include <string>
#include <iostream>
#include <openssl/sha.h>
#include <string.h>
using namespace std;

string sha256(const char* str_in, char* str_out)
{
   char buf[2];
   unsigned char hash[SHA256_DIGEST_LENGTH];
   SHA256_CTX sha256;
   SHA256_Init(&sha256);
   SHA256_Update(&sha256, str_in, strlen(str_in));
   SHA256_Final(hash, &sha256);
   std::string NewString = "";
   for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
   {
      sprintf(buf, "%02x", hash[i]);
      NewString = NewString + buf;
   }
   strcpy(str_out, NewString.c_str());
   return NewString;
}

int main()
{
   char str_in[300] = "hello!";
   char str_out[300];
   cout << sha256(str_in, str_out) << endl;
   printf("%s\n", str_out);
   return 0;
}