#include <stdio.h> 
#include <stdlib.h> 
#include <sys/types.h>  
#include <sys/socket.h>
#include <string.h> 
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 8080 
#define BUFF_SIZE 1024 

void handle_sigint();
void handle_sigpipe();

void* receiveMessage(void * socket) {
	int *sockfd, len;
	char buffer[BUFF_SIZE]; 
	sockfd = (int *) socket;
	memset(buffer, 0, BUFF_SIZE);  
	for (;;) {
		memset(buffer,0,strlen(buffer));
		len = recv(*sockfd, buffer, BUFF_SIZE, 0);
		if(!strcmp(buffer, "/exit")){
			if(kill(getpid(), SIGPIPE) < 0){
				exit(EXIT_FAILURE);
			}
		} else {
			if (len < 0) {  
				printf("Error receiving data!\n");    
			} else if(len > 0){
				printf("%s\n",buffer);
			}  
		}
	}
}

int sockfd;

int main(int argc, char**argv) {

	struct sockaddr_in addr;  
	int sent;  
	char buffer[BUFF_SIZE];
	pthread_t rThread; 

	// We create the socket. PF_INET - TCP/IP protocol family, SOCK_STREAM - connection socket, 0 - implicit protocol
	sockfd = socket(AF_INET, SOCK_STREAM, 0);  
	if (sockfd < 0) {  
		printf("Error creating socket!\n");  
		exit(EXIT_FAILURE);  
	}
	printf("**Socket created.\n");   

	struct sigaction sa;
	sa.sa_handler = &handle_sigint;
	sa.sa_flags = 0;
	sigfillset(&sa.sa_mask);
	if (sigaction(SIGINT, &sa, NULL) == -1) {
        	printf("Error setting handler for SIGINT");
		exit(EXIT_FAILURE); 
    	}

	struct sigaction sa2;
	sa2.sa_handler = &handle_sigpipe;
	sa2.sa_flags = 0;
	sigfillset(&sa2.sa_mask);
    	if (sigaction(SIGPIPE, &sa2, NULL) == -1) {
        	printf("Error setting handler for SIGPIPE"); 
		exit(EXIT_FAILURE);
    	}

	memset(&addr, 0, sizeof(addr));  
	addr.sin_family = AF_INET;  //address family
	addr.sin_addr.s_addr = inet_addr("127.0.0.1"); //internet address
	addr.sin_port = htons(PORT); //port in network; htons -> host to network short

	// We make a connection on the socket.
	sent = connect(sockfd, (struct sockaddr *) &addr, sizeof(addr));  
	if (sent < 0) {  
		printf("Error connecting to the server!\n");  
		exit(EXIT_FAILURE);  
	}
	printf("Connected to the chat.\n"); 	

	// We create a new thread for receiving messages from the server
	int ret = pthread_create(&rThread, NULL, receiveMessage,  &sockfd);
 	if (ret) {
  		printf("Error creating a new thread\n");
  		exit(EXIT_FAILURE);
	}		

	while (fgets(buffer, BUFF_SIZE, stdin) != NULL) {
		if(strlen(buffer)>BUFF_SIZE)
		{
			printf("Message longer than 1024 characters\n");
		}else{
			if(strcmp(buffer, "/exit\n") == 0){
				// /exit acts like Ctrl+C
				if(kill(getpid(), SIGINT) < 0){
					printf("SIGINT failed");
					exit(EXIT_FAILURE);
				}
			} else {
				sent = write(sockfd, buffer, strlen(buffer));
				if (sent < 0) {  
					printf("Error sending data!\n\t - %s", buffer);
				}
			}
		}
	}
	pthread_detach(pthread_self());

	return 0;
}

void exit_ctrl_c(){
	int len = 7;
	char* buffer = (char *)malloc(len*sizeof(char));
	snprintf(buffer, len, "%s", "/exit\n");
	int sent = write(sockfd, buffer, len);
	if (sent < 0) {  
		printf("Error sending data!\n\t-%s", buffer);
	}
}

void handle_sigint()
{
	exit_ctrl_c();
	shutdown(sockfd, SHUT_RDWR);
	exit(EXIT_FAILURE);
 	
}

void handle_sigpipe(){
	printf("Server got disconnected\n");
	shutdown(sockfd, SHUT_RDWR);
	exit(EXIT_FAILURE);
}
