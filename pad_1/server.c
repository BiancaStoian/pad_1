#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define MAX_USERS 50
#define FILE_NAME "users.in"
#define LOGOUT "/logout"
#define LOGIN "/login"
#define MESSAGE "/message"
#define OPTIONS "/options"

typedef struct user {
	char name[BUFFER_SIZE];
	char password[BUFFER_SIZE];
} credentials;

// client connection structure
typedef struct client{
	struct sockaddr_in addr;	/* Client remote address */
	int client_fd;			/* Connection file descriptor */
	int uid;			/* Client unique identifier */
	credentials user;		/* Client credentials */
} client_t;



credentials users[MAX_USERS];
int no_users;

int current_uid;
client_t *client_list[MAX_CLIENTS];

int connected_clients;

void add_client(client_t *client){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(!client_list[i]){
			client_list[i] = client;
			return;
		}
	}
	connected_clients++;
}

void remove_client(client_t *client){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(client_list[i]->uid == client->uid){
			client_list[i] = NULL;
			return;
		}
	}
	connected_clients--;
}

void read_users() {
	FILE *file;
   	file = fopen(FILE_NAME,"r");

	if( file == NULL ) {
		perror("Error while opening file.\n");
		exit(EXIT_FAILURE);
	}
	while( ( fscanf(file, "%s", users[no_users].name) != EOF ) && ( fscanf(file, "%s", users[no_users].password) != EOF )){
		no_users++;
	}

  	fclose(file);
}

int login(client_t *client, char *username, char *password){
	int i;
	for(i=0;i<no_users; i++){
		if(!strcmp(username, users[i].name) && !strcmp(password, users[i].password)){
			client->user = users[i];
			add_client(client);
			return 1;
		}
	}
	return 0;
}

int is_online(client_t *client){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(client_list[i] != NULL){
			if(client_list[i]->uid == client->uid){
				return 1;
			}
		}
	}
	return 0;
}

int send_message(int fd, char *message){
	printf("...%s...\n", message);
	return write(fd, message, strlen(message));
}

void send_message_to_all(char *message){
	int i;	
	for(i=0; i<MAX_CLIENTS; i++){
		client_t *client = client_list[i];
		if(client){
			send_message(client->client_fd, message);
		}
	}
}

void strip_newline(char *s){
	while(*s != '\0'){
		if(*s == '\r' || *s == '\n'){
			*s = '\0';
		}
		s++;
	}
}

void *handle_client(void *arg){
	client_t *client = (client_t*)arg;
	char buff_in[BUFFER_SIZE];
	int len;
	int state = 0;
	char password[BUFFER_SIZE];
	char username[BUFFER_SIZE];
	
	while(1){
		printf("%d\n", state);
		switch(state){
			case 1:
				
				send_message(client->client_fd, "username = ");
				len = recv(client->client_fd, buff_in, BUFFER_SIZE, 0);
				strip_newline(buff_in);
				strcpy(username, buff_in);
				printf("****%s***\n", username);
				send_message(client->client_fd, "password = ");
				len = recv(client->client_fd, buff_in, BUFFER_SIZE, 0);
				strip_newline(buff_in);
				strcpy(password, buff_in);

				if(!is_online(client)){
					if(login(client, username, password)){
						state = 2;
						int len = 29 + strlen(client->user.name);
						char *s  = (char *) malloc (len * sizeof(char)) ;
						snprintf(s, len, "User %s has joined the chat", client->user.name);		
						send_message_to_all(s);
						break;
					} else{
						send_message(client->client_fd, "Login failed. Please retry");
					}
				} else{
					send_message(client->client_fd, "The user is already logged in..");
				}
				state = 0;
				
			case 0:
				
				if(send_message(client->client_fd, 
					"Please use the following options:\n\t-/login\n\t-/message\n\t-/logout\n") < 0)
				{
					printf("send failed line 155");	
				}
				len = recv(client->client_fd, buff_in, BUFFER_SIZE, 0);
				if(len > 1){
					buff_in[len-2] = '\0';
				} else{
					buff_in[0] = '\0';
				}
				break;

			case 2:
				break;
			
				
		}
		if(state == 2){
			len = recv(client->client_fd, buff_in, BUFFER_SIZE, 0);
			strip_newline(buff_in);
			send_message_to_all(buff_in);
			
		}
		if(!strcmp(buff_in, LOGOUT)){
			break;
		} else if(!strcmp(buff_in, LOGIN)){
			state = 1;
			continue;
		} else if(!strcmp(buff_in, MESSAGE)){
			state = 2;
			continue;
		} else if(!strcmp(buff_in, OPTIONS)){
			state = 0;
		} 
		
	}
	len = 27 + strlen(client->user.name);
	char *s = (char *) malloc (len * sizeof(char));
	snprintf(s, len, "\nUser %s has left the chat", client->user.name);
	send_message_to_all(s);
	close(client->client_fd);
	remove_client(client);
	return NULL;
}

int main(int argc, char *argv[]){
	struct sockaddr_in server_addr;
	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(PORT);

	if(bind(socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0){
		perror("Socket binding failed");
		return 1;
	}

	if(listen(socket_fd, 20) < 0){
		perror("Socket listening failed");
		return 1;
	}

	printf("SERVER STARTED - PID - %d..........\n", getpid());
	read_users();
 	int client_fd;
 	struct sockaddr_in cli_addr;
	client_t *client;
 	socklen_t clilen;
	while(1){
 		clilen = sizeof(cli_addr);
		client_fd = accept(socket_fd, (struct sockaddr*)&cli_addr, &clilen);
		printf("%d\n", client_fd);
		if(client_fd > 0 && connected_clients < MAX_CLIENTS){
			client=(client_t*) malloc(sizeof(client_t));
			client->client_fd = client_fd;
			client->uid = current_uid++;
			client->addr = cli_addr;
			pthread_t thread_id;
			pthread_create(&thread_id, NULL, &handle_client, client);
           		//puts(s);
			pthread_detach(pthread_self());
		} else{
			perror("ana");
		}

		sleep(2);
	}
}
