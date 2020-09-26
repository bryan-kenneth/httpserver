#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include "httpserverlib.h"

#define LINE_SIZE 100

/******************************* QUEUE ****************************/
struct node *head = NULL;
struct node *tail = NULL;

void enqueue(int *client_ptr) {
	struct node *newnode = (struct node *)malloc(sizeof(struct node));
	newnode->client_fd = client_ptr;
	newnode->next = NULL;
	if (tail == NULL) {
		head = newnode;
	} else {
		tail->next = newnode;
	}
	tail = newnode;
} 

int *dequeue() {
	if (head == NULL) {
		return NULL;
	} else {
		int *result = head->client_fd;
		struct node *temp = head;
		head = head->next;
		if (head == NULL) {
			tail = NULL;
		}
		free(temp);
		return result;
	}
}

/******************************* LOG ******************************/
int offset = 0;
int log_fd;

pthread_mutex_t offset_lock = PTHREAD_MUTEX_INITIALIZER;

void open_log(char *address) {
	if ((log_fd = open(address, O_WRONLY | O_CREAT | O_TRUNC)) == -1) {
		fprintf(stderr, "Failed to create/open log\n");
		exit(EXIT_FAILURE);
	}
}

int new_valid_entry(struct Request r) {
	char buffer[LINE_SIZE];
	int length = r.length;
	if (r.command == GET) {
		sprintf(buffer, "GET %s length %d\n", r.address, length);
	} else {
		sprintf(buffer, "PUT %s length %d\n", r.address, length);
	}
	
	int size = strlen(buffer);
	while (length > 0) {
		if (length < 20) {
			size += 8 + (3 * length) + 1;
		} else {
			size += 69; // full 20 byte line
		}
		length -= 20;
	}
	size += 17; // for ========\n
	
	r.log_offset = -1;
	while (r.log_offset == -1) {
		pthread_mutex_lock(&offset_lock);
		r.log_offset = offset;
		offset += size;
		pthread_mutex_unlock(&offset_lock);
	}
	
	printf("%d\n", r.log_offset);
	printf("printing to log\n");
	pwrite(log_fd, buffer, strlen(buffer), r.log_offset);
	r.log_offset += strlen(buffer);
	
	return r.log_offset;
}


/***************** CONNECTION HANDLING ****************************/
struct Request ParseRequest(char *buffer, bool log) {
	struct Request r;
	char contlen_str[] = "Content-Length: ";
	char delim[] = " /";
	
	// find content-length if availible
	// done first because strtok is destructive
	char *ret = strstr(buffer, contlen_str);
	
	// parse requested command
	char *com = strtok(buffer, delim);
	if ((strcmp(com, "GET") == 0)) {
		r.command = GET;
	} else if ((strcmp(com, "PUT") == 0)) {
		r.command = PUT;
	} else {
		r.valid = false;
		r.command = ERR;
		return r;// unknown command, no need to parse further
	}
	
	// parse address
	r.address = strtok(NULL, delim);
		
	// check address validity
	r.valid = false;
	if (strlen(r.address) == 27) { // 27 characters
		for (int i = 0; i < 27; i++) {
			// Check range of each character
			if ((r.address[i] < '0' && r.address[i] != '-') ||
				(r.address[i] > '9' && r.address[i] < 'A') ||
				(r.address[i] > 'Z' && r.address[i] < 'a' && r.address[i] != '_') ||
				(r.address[i] > 'z')) {
				r.valid = false;
				return r;
			}
		}
		r.valid = true;
	}
	
	// attempt getting file descriptor
	r.created = false;
	/***** GET *****/
	if (r.command == GET) {
		// attempt openning file
		if ((r.fd = open(r.address, O_RDONLY)) == -1) {
			r.valid = false; // cant open
			if (errno == EACCES) {
				r.fd = PRIVATE; // private
			}
			return r;
		}
		
		// parse Content-Length
		struct stat file_stats;
		fstat(r.fd, &file_stats);
		r.length = file_stats.st_size;
		if (log) {
			r.log_offset = new_valid_entry(r);
			r.log_line = 0;
		}
		
	/***** PUT *****/
	} else {
		// attempt creating file
		if ((r.fd = open(r.address, O_WRONLY | O_CREAT | O_EXCL)) == -1) {
			// already exists
			if (errno == EEXIST) {
				// open and clear existing file
				r.fd = open(r.address, O_WRONLY | O_TRUNC);
			} else if (errno == EACCES) {
				r.fd = PRIVATE; // private
				r.valid = false;
				return r;
			}
		} else {
			r.created = true;
		}
		
		// parse Content-Length if availible
		if (ret != NULL) {
			r.length = atoi(ret + strlen(contlen_str));
			if (log) {
				r.log_offset = new_valid_entry(r);
				r.log_line = 0;
			}
		}
	}
	printf("r offset: %d\n", r.log_offset);
	return r;
}


void httpGET(int client_fd, struct Request r, char *buffer, bool log) {	
	// send header to client
	int wcount = sprintf(buffer, "%s %d\r\n\r\n", MSG[OK_CONT], r.length);
	write(client_fd, buffer, wcount);
			
	// bytes to be read count
	int rcount = BUFFER_SIZE;
	while (r.length > 0) {
		if (r.length < BUFFER_SIZE)
			rcount = r.length;

		// read and write data
		wcount = read(r.fd, buffer, rcount);
		write(client_fd, buffer, wcount);
		
		if (log) {
			//
		}
			
		// update bytes left
		r.length -= wcount;
	}
	
	// close file
	close(r.fd);
}


bool httpPUT(int client_fd, struct Request r, char *buffer, bool log) {
	// 100 Continue
	int wcount = strlen(MSG[CONTINUE]);
	write(client_fd, MSG[CONTINUE], wcount);
	
	// number of bytes to be read
	int read_count = BUFFER_SIZE;
	while (r.length > 0) {
		if (r.length < BUFFER_SIZE)
			read_count = r.length;
					
		// read and write data
		wcount = read(client_fd, buffer, read_count);
		write(r.fd, buffer, wcount);
		if (log) {
			// 
		}
		
		// update bytes left
		r.length -= BUFFER_SIZE;
	}
	
	// close file
	close(r.fd);
	return r.created;
}





