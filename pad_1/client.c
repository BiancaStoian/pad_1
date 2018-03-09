#include"stdio.h"  
#include"stdlib.h"  
#include"sys/types.h"  
#include"sys/socket.h"  
#include"string.h"  
#include"netinet/in.h"  
#include"netdb.h"
#include"pthread.h"
#include"assert.h"
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080 
#define BUF_SIZE 150 

void removeSubstring(char *s,const char *toremove)
{
  while( (s = strstr(s, toremove)) )
    memmove(s,s+strlen(toremove),1+strlen(s+strlen(toremove)));
}

void * receiveMessage(void * socket) {
	int *sockfd, len;
	char buffer[BUF_SIZE]; 
	sockfd = (int *) socket;
	memset(buffer, 0, BUF_SIZE);  
	for (;;) {
		memset(buffer,0,strlen(buffer));
		len = recv(*sockfd, buffer, BUF_SIZE, 0);
		
		if (len < 0) {  
			printf("Error receiving data!\n");    
		} else if(len > 0){
			printf("%s\n",buffer);
		}  
	}
}

int main(int argc, char**argv) {

	struct sockaddr_in addr;  
	int sockfd, sent;  
	char buffer[BUF_SIZE];
	pthread_t rThread; 

	sockfd = socket(AF_INET, SOCK_STREAM, 0);  
	if (sockfd < 0) {  
		printf("Error creating socket!\n");  
		exit(1);  
	}
	printf("Socket created...\n");   

	memset(&addr, 0, sizeof(addr));  
	addr.sin_family = AF_INET;  
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(PORT);     

	sent = connect(sockfd, (struct sockaddr *) &addr, sizeof(addr));  
	if (sent < 0) {  
		printf("Error connecting to the server!\n");  
		exit(-1);  
	}

	int ret = pthread_create(&rThread, NULL, receiveMessage,  &sockfd);
 	if (ret) {
  		printf("ERROR: Return Code from pthread_create() is %d\n", ret);
  		exit(1);
	}		

	while (fgets(buffer, BUF_SIZE, stdin) != NULL) {
		//printf("%s\n",buffer);
		if(strlen(buffer)>150)
		{
			printf("Error! Message longer than 150 characters\n");
		}else{
			strcat(buffer, "\n");
			sent = write(sockfd, buffer, strlen(buffer));
			if (sent < 0) {  
				printf("Error sending data!\n\t-%s", buffer);
			} 
		}
	}

	close(sockfd);

}
