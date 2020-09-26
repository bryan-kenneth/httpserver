/**************************************************************************
File: httplib.cpp

Description:
Support library for http server with caching and logging
***************************************************************************/

#include <sys/stat.h>
#include <errno.h>
#include "httpserver.h"

#define IN  1
#define OUT 0

// Local function prototypes
void OpenServerFile(Request *R);
void CacheTransfer(Request *R);
void FileTransfer(Request *R, int dir);

/*** Global Functions *****************************************************/

/* ReadAndParse reads in and parses client request buffer */
Request ReadAndParse(int client_fd) {
	// New request struct
	Request r;
	r.c_fd = client_fd;
	// read request
	char req_buff[H_SIZE];
	if (read(r.c_fd, req_buff, H_SIZE) < 0) {
		fprintf(stderr, "Read client header failed\n");
		exit(EXIT_FAILURE);
	}
	// find content-length if availible
	// done first because strtok is destructive
	char cl_str[] = "Content-Length: ";
	char *cl_ptr = strstr(req_buff, cl_str);
	
	// parse requested command
	char d[] = " /";
	char *ptr;
	ptr = strtok(req_buff, d);
	stpcpy(r.cmd, ptr);
	if ((strcmp(r.cmd, "GET") == 0)) {
		r.reply = GET;
	} else if ((strcmp(r.cmd, "PUT") == 0)) {
		r.reply = PUT;
	} else {
		// unknown command, no need to parse further
		r.reply = R500;
		return r;
	}
	// parse addr
	ptr = strtok(NULL, d);
	stpcpy(r.addr, ptr);
	// check addr validity
	if (strlen(r.addr) == 27) { // 27 characters
		for (int i = 0; i < 27; i++) {
			// Check range of each character
			if ((r.addr[i] < '0' && r.addr[i] != '-') ||
				(r.addr[i] > '9' && r.addr[i] < 'A') ||
				(r.addr[i] > 'Z' && r.addr[i] < 'a' && r.addr[i] != '_') ||
				(r.addr[i] > 'z')) {
				// invalid address, no need to parse further
				r.reply = R400;
				return r;
			}
		}
	}
	// parse Content-Length if availible
	if (cl_ptr != NULL) {
		r.length = atoi(cl_ptr + strlen(cl_str));
	}
	// process server side file
	OpenServerFile(&r);
	
	return (r);
}

/* httpGET handles GET requests with logging and caching support */
void httpGET (Request *R) {
	char data_buff[D_SIZE];
	
	// send header to client
	sprintf(data_buff, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", R->length);
	write(R->c_fd, data_buff, strlen(data_buff));
	
	// transfer
	bool cf = CacheFile(R);
	LogRequest(R, cf);
	if (cf) {
		CacheTransfer(R);
	} else {
		FileTransfer(R, OUT);
	}
	close(R->s_fd);
	
	// set reply
	R->reply = R200;
}

/* httpPUT handles PUT requests with logging and caching support */
void httpPUT (Request *R) {
	char data_buff[D_SIZE];
	
	// send continue to client
	sprintf(data_buff, "HTTP/1.1 100 Continue\r\n\r\n");
	write(R->c_fd, data_buff, strlen(data_buff));
	
	// transfer
	bool cf = CacheFile(R);
	LogRequest(R, cf);
	FileTransfer(R, IN);
	close(R->s_fd);
	
	// set reply
	if (R->reply == PUTC) {
		R->reply = R201;
	} else {
		R->reply = R200;
	}
}

/* Sends reply to client dependant on reply value in Request structure */
void SendReply (Request *R) {
	char res_buff[H_SIZE];
	switch (R->reply) {
		case R200:
			sprintf(res_buff, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
			break;
		case R201:
			sprintf(res_buff, "HTTP/1.1 201 Created\r\nContent-Length: 0\r\n\r\n");
			break;
		case R400:
			sprintf(res_buff, "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n");
			break;
		case R403:
			sprintf(res_buff, "HTTP/1.1 403 Forbiden\r\nContent-Length: 0\r\n\r\n");
			break;
		case R404:
			sprintf(res_buff, "HTTP/1.1 404 File not found\r\nContent-Length: 0\r\n\r\n");
			break;
		case R500:
			sprintf(res_buff, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n");
			break;
	}
			
	if (write(R->c_fd, res_buff, strlen(res_buff)) < 0) {
		fprintf(stderr, "Response to client failed\n");
		exit(EXIT_FAILURE);
	}
}

/*** Local Functions ******************************************************/

/* OpenServerFile assumes request address is valid and attempts to open or
 * create the server side file */
void OpenServerFile(Request *R) {
	if (R->reply == GET) {
		// attempt openning file
		if ((R->s_fd = open(R->addr, O_RDONLY)) == -1) {
			if (errno == EACCES) { // if file is private
				R->reply = R403;
			} else {
				R->reply = R404;
			}
		} else {
			// parse Content-Length
			struct stat s;
			fstat(R->s_fd, &s);
			R->length = s.st_size;
		}
	} else /* PUT */ { 
		// attempt creating file
		if ((R->s_fd = open(R->addr, O_WRONLY | O_CREAT | O_EXCL)) == -1) {
			if (errno == EEXIST) { // if file already exists
				// open and clear existing file
				R->s_fd = open(R->addr, O_WRONLY | O_TRUNC);
			} else if (errno == EACCES) { // if file is private
				R->reply = R403;
			} else {
				fprintf(stderr, "PUT open server file failed\n");
				exit(EXIT_FAILURE);
			}
		} else {
			// file created
			R->reply = PUTC;
		}
	}	
}

/* CacheTransfer copies data from cache and writes to client
 * Assumes httpcache.cpp is ready for CacheDataOut transfers */
void CacheTransfer(Request *R) {
	char data_buff[D_SIZE];
	int tcount = D_SIZE, length = R->length;
	while (length != 0) {
		if (length < D_SIZE) {
			tcount = length;
		}
		
		CacheDataOut(data_buff, tcount);
		write(R->c_fd, data_buff, tcount);
		length -= tcount;
	}
	
}

/* FileTransfer reads and writes data acording to content length
 * dir specifies direct, IN (client to server) or OUT (server to client) */
void FileTransfer(Request *R, int dir) {	
	char data_buff[D_SIZE];
	int wcount, rcount = D_SIZE, length = R->length;
	while (length != 0) {
		if (length < D_SIZE) {
			rcount = length;
		}
		
		if (dir == IN) {
			wcount = read(R->c_fd, data_buff, rcount);
			write(R->s_fd, data_buff, wcount);
		} else {
			wcount = read(R->s_fd, data_buff, rcount);
			write(R->c_fd, data_buff, wcount);
		}
		
		LogData(data_buff, wcount);
		CacheDataIn(data_buff, wcount);
		length -= wcount;
	}
	LogDataEnd();
}
