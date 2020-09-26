#ifndef HEADERFILE_H
#define HEADERFILE_H

// COMMAND
#define ERR	   -1
#define GET		1
#define PUT  	2

// FD
#define NOT_FOUND	-1
#define PRIVATE	 	-2

// STATUS CODE STRING INDEX
#define CONTINUE	0
#define OK			1
#define CREATED   	2
#define BADREQ		3
#define FORBIDDEN	4
#define NOTFOUND	5
#define SERVERERR	6
// Note: must provide content length for OK_CONT
#define OK_CONT		7

#define BUFFER_SIZE 4080

// constant array of status code strings
static const char *MSG[] = {
	"HTTP/1.1 100 Continue\r\n\r\n",
	"HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n",
	"HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n",
	"HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n",
	"HTTP/1.1 403 Forbiden\r\nContent-Length: 0\r\n\r\n",
	"HTTP/1.1 404 File not found\r\nContent-Length: 0\r\n\r\n",
	"HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n",
	"HTTP/1.1 200 OK\r\nContent-Length:"
};

// structure for queue
struct node {
	struct node *next;
	int *client_fd;
};

struct Log {
	bool fail;
	
};

// structure for request message parsing
struct Request {
	int command;
	char *address;
	bool valid;
	int length;
	bool created;
	int fd;
	int log_offset;
	int log_line;
};

// function declarations
void enqueue(int *client_fd);
int *dequeue(void);

void open_log(char *address);
int new_valid_entry(struct Request r);


struct Request ParseRequest(char *buffer, bool log);
void httpGET(int client_fd, struct Request r, char *buffer, bool log);
bool httpPUT(int client_fd, struct Request r, char *buffer, bool log);

#endif

