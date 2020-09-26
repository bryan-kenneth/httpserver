/**************************************************************************
File: httpserver.h

Description:
Header file for single threaded http server with loggging and caching
***************************************************************************/

#ifndef HEADERFILE_H
#define HEADERFILE_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

// REPLY
#define GET   -1
#define PUT   -2 
#define PUTC  -3
#define R200 200 // OK
#define R201 201 // Created
#define R400 400 // Bad Request (Invalid Address)
#define R403 403 // Forbidden (Private File)
#define R404 404 // File Not Found
#define R500 500 // Internal Server Error (Unknown Command)

// Buffer Sizes
#define L_SIZE 100  // line
#define H_SIZE 200  // header
#define D_SIZE 4000 // data

// structure for processing requests
typedef struct {
	int reply;
	char cmd[10];
	char addr[50];
	int length;
	int c_fd;
	int s_fd;
} Request;

/*** Function Prototypes ***/
// httplib.cpp
Request ReadAndParse(int client_fd);
void httpGET (Request *R);
void httpPUT (Request *R);
void SendReply (Request *R);

// httplog.cpp
void OpenLog(char *log_name);
void LogRequest(Request *R, bool cached);
void LogData(char *data, int count);
void LogDataEnd(void);

// httpcache.cpp
void CacheInit(void);
bool CacheFile(Request *R);
void CacheDataIn(char *data, int count);
void CacheDataOut(char *data, int count);
void PrintCache(void);

#endif

