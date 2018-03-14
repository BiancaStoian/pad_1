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
#include <signal.h>

#define PORT 8080
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024
#define MAX_USERS 50
#define FILE_NAME "users.in"
#define LOGOUT "/logout"
#define LOGIN "/login"
#define EXIT "/exit"

void handle_sigint();
void set_handler_sigint ();

typedef struct user {
	char name[BUFFER_SIZE];
	char password[BUFFER_SIZE];
} credentials;

// client connection structure
typedef struct client{
	struct sockaddr_in addr;	/* Client remote address */
	int client_fd;			/* Connection socket */
	int uid;			/* Client unique identifier */
	credentials user;		/* Client credentials */
} client_t;



credentials users[MAX_USERS]; //array containing all possible users
int no_users; 		      // number of possibe users (from the file)
int connected_clients;        // number of connected users (logged in)
int connected_to_chat;        // number of connected users (to chat)
int current_uid;              // used to generate unique ids for clients 
client_t *client_list[MAX_CLIENTS]; //data about every client that is currently logged in

// add clients to list
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


// remove clients from list (when the client logs out)
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

// takes number of users from our file
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

// checks if the password and username match to the ones existing in the list of possible users.
// if they match, the client is logged in
int login(client_t *client, char *username, char *password){
	int i;
	for(i=0;i<no_users; i++){
		if(!strcmp(username, users[i].name) && !strcmp(password, users[i].password)){
			client->user = users[i];
			strcpy(client->user.name, users[i].name);
			add_client(client);
			return 1;
		}
	}
	return 0;
}

//checks if the user is online
int is_online(char *username){
	int i;
	for(i=0;i<MAX_CLIENTS;i++){
		if(client_list[i] != NULL){
			if(!strcmp(client_list[i]->user.name, username)){
				return 1;
			}
		}
	}
	return 0;
}

// sends messages to the user
void send_message(int fd, char *message){
	if(write(fd, message, strlen(message)) < 0){
		printf("Error sending message\n");
		exit(EXIT_FAILURE);
	}
}

// sends messages to all the other users
// needs_name is 1 when we need to add the name of the sender of the message 
void send_message_to_all(char *message, client_t *current_client, int needs_name){
	if(strlen(message) == 0){
		return;
	}
	if(message[0] == '/'){
		return;
	}
	int i;	
	for(i=0; i<MAX_CLIENTS; i++){
		client_t *client = client_list[i];
		if(client && (current_client != NULL && (client->client_fd != current_client->client_fd))){
			if (needs_name == 1){
				char *new_message;
				int len = strlen(message) + 2 + strlen(current_client->user.name) + 2;
				new_message = (char *) malloc(len * sizeof(char));
				snprintf(new_message, len, "*%s: %s", current_client->user.name, message);
				send_message(client->client_fd, new_message);
			} else {
				send_message(client->client_fd, message);
			}
		}
	}
}

// cuts new lines at the end of the message
void strip_newline(char *s){
	while(*s != '\0'){
		if(*s == '\r' || *s == '\n'){
			*s = '\0';
		}
		s++;
	}
}


void logout(client_t *client){
	int len = 29 + strlen(client->user.name);
	char *s = (char *) malloc (len * sizeof(char));
	snprintf(s, len, "\n**User %s has left the chat\n", client->user.name);
	send_message_to_all(s, client, 0);
	remove_client(client);
}

void exit_client (client_t *client){
	if (is_online(client->user.name)){
		logout(client);
	}
	connected_to_chat--;
	close(client->client_fd);
}

void *handle_client(void *arg){
	client_t *client = (client_t*)arg;
	char buff_in[BUFFER_SIZE];
	int len;
	int state = 0;
	char password[BUFFER_SIZE];
	char username[BUFFER_SIZE];
	char message[BUFFER_SIZE];
	while(1){
		switch(state){
			case 1:  //log in state
				if(!is_online(client->user.name)){
					send_message(client->client_fd, "username = ");
					len = recv(client->client_fd, buff_in, BUFFER_SIZE, 0);
					if (len < 0) {
						printf("Error receiving data from client\n");
						exit(EXIT_FAILURE);
					}
					strip_newline(buff_in);
					strcpy(username, buff_in);
					send_message(client->client_fd, "password = ");
					len = recv(client->client_fd, buff_in, BUFFER_SIZE, 0);
					if (len < 0) {
						printf("Error receiving data from client\n");
						exit(EXIT_FAILURE);
					}
					strip_newline(buff_in);
					strcpy(password, buff_in);
					if(!is_online(username)){ //if the client has successfully logged in, he will be able to send messages
						if(login(client, username, password)){
							state = 2;
							int len = 31 + strlen(client->user.name);
							char *s  = (char *) malloc (len * sizeof(char)) ;
							snprintf(s, len, "\n**User %s has joined the chat\n", client->user.name);					
							send_message_to_all(s, client, 0);
							send_message(client->client_fd, "\n**You are now connected to the chat.\n");
							break;
						} else{
							send_message(client->client_fd, "\n**Login failed.\n");
						}
					} else{
						send_message(client->client_fd, "\n**The user is already logged in.\n");
					}
					state = 0;
				} else{                  //if the user is already logged in
					state = 2;
					send_message(client->client_fd, "\n**You are already logged in.\n");
				}
				
			case 0:  // state in which the client chooses what he wantes to do
				if(!is_online(client->user.name)){
					sprintf(message, "\nPlease use the following options:\n\t-/login\n\t-/exit\n");
				} else{
					sprintf(message, "\nPlease use the following options:\n\t-/logout\n\t-/exit\n");
				}
				send_message(client->client_fd, message);
				len = recv(client->client_fd, buff_in, BUFFER_SIZE, 0);
				if (len < 0) {
					printf("Error receiving data from client\n");
					exit(EXIT_FAILURE);
				}
				strip_newline(buff_in);
				break;

			case 2: // state for sending messages
				len = recv(client->client_fd, buff_in, BUFFER_SIZE, 0);
				if (len < 0) {
					printf("Error receiving data from client\n");
					exit(EXIT_FAILURE);
				}
				strip_newline(buff_in);
				send_message_to_all(buff_in, client, 1);
				break;
		}
		if(!strcmp(buff_in, LOGOUT) && is_online(client->user.name)){
			state = 0;
			logout(client);
			send_message(client->client_fd, "\n**You left the chat.\n");
		} else if(!strcmp(buff_in, LOGIN)){
			if (!is_online(client->user.name)) {
				state = 1;
			} else {
				send_message(client->client_fd, "\n**You are already logged in.\n");
			}
		} else if(!strcmp(buff_in, EXIT)){
			exit_client(client);
			shutdown(client->client_fd, SHUT_RDWR);       //it will block both sending and receiving data 
			break;
		}

	}
	return NULL;
}

int socket_fd;

int main(int argc, char *argv[]){
	struct sockaddr_in server_addr;
	socket_fd = socket(AF_INET, SOCK_STREAM, 0);
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
		
	set_handler_sigint ();
	printf("SERVER STARTED - PID - %d..........\n", getpid());
	read_users();
 	int client_fd;
 	struct sockaddr_in cli_addr;
	client_t *client;
 	socklen_t clilen;
 	int err;
	while(1){
 		clilen = sizeof(cli_addr);
		client_fd = accept(socket_fd, (struct sockaddr*)&cli_addr, &clilen);
		printf("%d\n", client_fd);
		if (client_fd < 0) {
			printf("Couldn't connect to the client\n");
			exit(EXIT_FAILURE);
		}
		if (connected_to_chat == MAX_CLIENTS){
			printf("Too many clients connected to the chat. Try later/n");
			exit(EXIT_FAILURE);
		}
		connected_to_chat++;
		client=(client_t*) malloc(sizeof(client_t));
		client->client_fd = client_fd;
		client->uid = current_uid++;
		client->addr = cli_addr;
		pthread_t thread_id;

		err = pthread_create(&thread_id, NULL, &handle_client, client);
   		if(err < 0){
   			printf("Failed to create thread");
			continue;
   		}
		pthread_detach(pthread_self());
		sleep(2);
	}
	return 0;
}

void remove_all_clients(){
	int i;
	for(i = 0; i < MAX_CLIENTS; i++){
		if(client_list[i]){
			shutdown(client_list[i]->client_fd, SHUT_RDWR);
			client_list[i] = NULL;
		}
	}
}

void sendExit(char *message){
	int i;
	for(i=0; i < MAX_CLIENTS; i++){
		if(client_list[i] != NULL){
			send_message(client_list[i]->client_fd, message);
		}
	}
}

void handle_sigint()
{
	sendExit("/exit");
	remove_all_clients();
	shutdown(socket_fd, SHUT_RDWR);
 	exit(0);
}

void set_handler_sigint (){
	struct sigaction sa;
	sa.sa_handler = &handle_sigint;
	sa.sa_flags = 0;
	sigfillset(&sa.sa_mask);
	if (sigaction(SIGINT, &sa, NULL) == -1) {
        	printf("Error: cannot handle SIGINT"); 
		exit(EXIT_FAILURE);
    	}
}
