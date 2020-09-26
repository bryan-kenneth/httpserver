/**************************************************************************
File: httplog.cpp

Description: 
***************************************************************************/

#include "httpserver.h"

// Struct for log
typedef struct {
	bool en = false;
	int  fd;
	int  off;
	int  line;
} Log;

// Log Variables
bool log_enable = false;
Log *L;

/*** Global Functions *****************************************************/

/* OpenLog allocates memory for a Log data structure,
 * initializes variables, and enables logging for httpserver */
void OpenLog(char *log_name){
	L = (Log *)malloc(sizeof(Log));
	if ((L->fd = open(log_name, O_WRONLY | O_CREAT | O_TRUNC)) < 0) {
		fprintf(stderr, "Failed to open/create log\n");
		exit(EXIT_FAILURE);
	}
	L->off = 0;
	L->line = 0;
	log_enable = true;
}

/* LogRequest creates and writes the header line for log entries */
void LogRequest(Request *R, bool cached) {
	if (log_enable) {
		char line_buff[L_SIZE];
		if (R->reply < 0) {
			if (cached) {
				if (R->reply == GET) {
					sprintf(line_buff, "GET %s length %d [was in cache]\n========\n", R->addr, R->length);
				} else {
					sprintf(line_buff, "PUT %s length %d [was in cache]\n", R->addr, R->length);
				}
			} else {
				sprintf(line_buff, "%s %s length %d [was not in cache]\n", R->cmd, R->addr, R->length);
			}
		} else {
			sprintf(line_buff, "FAIL: %s %s HTTP/1.1 --- Response %d\n========\n", R->cmd, R->addr, R->reply);
		}
		
		pwrite(L->fd, line_buff, strlen(line_buff), L->off);
		L->off += strlen(line_buff);
		L->line = 0;
	}
}

/* LogData formats and writes transfer data for log entries */
void LogData(char *data, int count) {
	if (log_enable) {
		char line_buff[L_SIZE];
		for (int i = 19; i <= count; i += 20, L->line += 20){
			sprintf(line_buff, "%08d %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", L->line,
					data[i-19], data[i-18], data[i-17], data[i-16], data[i-15],
					data[i-14], data[i-13], data[i-12], data[i-11], data[i-10],
					data[i- 9], data[i- 8], data[i- 7], data[i- 6], data[i- 5],
					data[i- 4], data[i- 3], data[i- 2], data[i- 1], data[i]);
			pwrite(L->fd, line_buff, strlen(line_buff), L->off);
			L->off += strlen(line_buff);
		}
		
		if (count < D_SIZE) {
			sprintf(line_buff, "%08d", L->line);
			pwrite(L->fd, line_buff, strlen(line_buff), L->off);
			L->off += strlen(line_buff);
			for (int i = 0; i <= (count %20); i++) {
				sprintf(line_buff, " %02x", data[i]);
				pwrite(L->fd, line_buff, strlen(line_buff), L->off);
				L->off += strlen(line_buff);
			}
		}
	}
}

/* LogDataEnd appends "========" to log */
void LogDataEnd(void) {
	if (log_enable) {
		char line_buff[L_SIZE];
		sprintf(line_buff, "========\n");
		pwrite(L->fd, line_buff, strlen(line_buff), L->off);
		L->off += strlen(line_buff);
	}
}
