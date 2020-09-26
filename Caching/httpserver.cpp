/**************************************************************************
File: httpserver.cpp

Description:
Main initialization and loop for http server with caching and logging
***************************************************************************/
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "httpserver.h"

int main (int argc, char *argv[]) {
	
	/********************* parsing command arguments **********************/
	// defaults
	int port = 80;

	int opt;
	while((opt = getopt(argc, argv, "p:cl:")) != -1) {
		switch(opt) {
			case 'p':
				port = atoi(optarg);
				break;
				
			case 'c': // Caching
				CacheInit();
				break;
			
			case 'l':
				OpenLog(optarg);
				break;
			
			default:
				printf("works\n\n");
				break;
		}
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
		
		// parse request
		Request req = ReadAndParse(client_fd);
		
		if (req.reply < 0) {
			if (req.reply == GET) {
				httpGET(&req);
			} else {
				httpPUT(&req);
			}
		} else {
			LogRequest(&req, false);
		}

		SendReply(&req);
		
		// close client
		close(client_fd);
	}
}

