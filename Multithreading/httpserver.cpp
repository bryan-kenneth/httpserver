#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include "httpserverlib.h"

#define MAX_THREADS 25

// function declarations
void * thread_function(void *); 
void handle_connection(int *client_fd);

// mutex locks and condition
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t condition_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_cond = PTHREAD_COND_INITIALIZER;

// if log specified true
bool log = false;

int main (int argc, char *argv[]) {
	
	/********************* parsing command arguments **********************/
	// defaults
	int port = 80;
	int threads = 4;
	
	if (argc == 1) {
		printf("No Port specified. Default: Port 80\n");
	} else {
		port = atoi(argv[1]);
		for (int i = 2; i < argc; i++) {
			if ((strcmp(argv[i], "-N") == 0)) {
				threads = atoi(argv[i + 1]);
			} else if ((strcmp(argv[i], "-l") == 0)) {
				open_log(argv[i + 1]);
				log = true;
			}
		}
	}
	
	/************************** creating threads **************************/
	pthread_t thread_pool[MAX_THREADS];
	
	// create threads to handle future connections
	for (int i = 0; i < threads; i++) {
		pthread_create(&thread_pool[i], NULL, thread_function, NULL);
	}
	
	/************************** setting up server *************************/
	int server_fd, client_fd; // socket file descriptors for server and client
	struct sockaddr_in server, client; // socket address structures
	int client_len = sizeof(client);

	// creating socket file descriptor
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Socket failed");
		exit(EXIT_FAILURE);
	}

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(port);

	// attaching socket to port
	if (bind(server_fd, (struct sockaddr *) &server, sizeof(struct sockaddr_in)) < 0) {
		fprintf(stderr, "Bind failed\n");
		exit(EXIT_FAILURE);
	}
	
	// listen on port
	if (listen(server_fd, 3) < 0) {
		fprintf(stderr, "Listen failed\n");
		exit(EXIT_FAILURE);
	} else {
		printf("Server is listening on %d\n", port);
	}
	
	/************************* forever loop *****************************/
	while (true) {
		// establish connection
		if ((client_fd = accept(server_fd, (struct sockaddr *) &client, (socklen_t*) &client_len)) < 0) {
			fprintf(stderr, "Accept failed\n");
			exit(EXIT_FAILURE);
		}
		
		int *client_ptr = (int *) malloc(sizeof(int));
		*client_ptr = client_fd;
		
		// add connection to queue
		// queue is a shared resource
		pthread_mutex_lock(&mutex);
		enqueue(client_ptr);
		pthread_mutex_unlock(&mutex);
		
		// signal to check for connections
		pthread_cond_signal(&condition_cond);
	}
	return (0);
}


void * thread_function(void *) {
	while (true) {
		int *client_fd = NULL;
		
		// condition used to prevent busy waiting
		pthread_cond_wait(&condition_cond, &condition_mutex);
		pthread_mutex_unlock(&condition_mutex);
		
		// queue is a shared resource
		pthread_mutex_lock(&mutex);
		client_fd = dequeue();
		pthread_mutex_unlock(&mutex);
		
		if (client_fd != NULL) {
			handle_connection(client_fd);
		} else {
			// no more connections, stop checking
			pthread_mutex_lock(&condition_mutex);
		}
	}
}

void handle_connection(int *client_ptr) {
	int client_fd = *((int *) client_ptr);
	free(client_ptr);
	char buffer[BUFFER_SIZE];
		
	// read request
	int rcount = read(client_fd, buffer, BUFFER_SIZE);
	buffer[rcount] = '\0';
		
	// parse request (function defined in httpserverlib.cpp)
	struct Request req = ParseRequest(buffer, log);
	
	int wcount;
	
	if (req.valid) {
		
		/***** GET ******/
		if (req.command == GET) {
			// preform GET (function defined in httpserverlib.cpp)
			httpGET(client_fd, req, buffer, log);
				
			// final response
			wcount = sprintf(buffer, "%s", MSG[OK]);
		}
		
		/***** PUT *****/
		else {
			// preform PUT (function defined in httpserverlib.cpp)
			if (httpPUT(client_fd, req, buffer, log)) {
				wcount = sprintf(buffer, "%s", MSG[CREATED]);
			} else {
				wcount = sprintf(buffer, "%s", MSG[OK]);
			}
		}
	}
		
	/***** ERRORS *****/
	else {
		if (req.command == ERR) {
			// unkown command
			wcount = sprintf(buffer, "%s", MSG[SERVERERR]);
		} else if (req.fd == PRIVATE) {
			// private file
			wcount = sprintf(buffer, "%s", MSG[FORBIDDEN]);
		} else if (req.fd == NOT_FOUND) {
			// file not found
			wcount = sprintf(buffer, "%s", MSG[NOTFOUND]);
		} else {
			// invalid address
			wcount = sprintf(buffer, "%s", MSG[BADREQ]);
		}
	}
		
	// send final response back to client
	if (write(client_fd, buffer, wcount) < 0) {
		fprintf(stderr, "Response to client failed\n");
		exit(EXIT_FAILURE);
	}
		
	// close client
	close(client_fd);
}




