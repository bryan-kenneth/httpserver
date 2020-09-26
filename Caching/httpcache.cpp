/**************************************************************************
File: httpcache.cpp

Description: Circular linked-list impementation of cache for httpserver
***************************************************************************/
#include "httpserver.h"

#define MAX  4
#define NONE 0

// Struct for File Node
typedef struct File_Node {
	struct File_Node *next;
	char  name[28];
	char  *data;
	char  *ptr;
} File_Node;

// Struct for File Cache
typedef struct {
	int nfiles; //number of files
	File_Node  *start;
} File_Cache;

// Cache variables
bool cache_enable = false;
File_Cache *C;
File_Node  *P; // for cache out transfers

// Local function prototypes
File_Node * CacheSearch(char *filename);
void CacheRemove(File_Node *FN);
void CacheInsert(File_Node *FN);

/*** Global Functions *****************************************************/

/* CacheInit allocates memory for a File Cache data structure,
 * initializes variables, and enables caching for httpserver */
void CacheInit (void) {
	C = (File_Cache *)malloc(sizeof(File_Cache));
	C->nfiles = NONE;
	C->start = NULL;
	cache_enable = true;
}

/* PrintCache prints all of the file names in cache */
 void PrintCache(void) {
	File_Node *FN = C->start;
	printf("\n====== %d Cache Files ======\n", C->nfiles);
	for (int i = 0; i < 2*C->nfiles; i++, FN = FN->next) {
		printf("%s\n", FN->name);
	}
	printf("===========================\n\n");
 }

/* CacheFile removes outdated files when necessary and inserts a new File Node
 * If GET request and file is already in cache Data Point P set to located file
 * Returns true if the file was already in cache and false otherwise */
bool CacheFile(Request *R) {
	// only when cache enabled
	if (cache_enable) {
		// if file already in cache
		if ((P = CacheSearch(R->addr)) != NULL) {
			if (R->reply == GET) {
				// set File Node P for CacheDataOut
				P->ptr = P->data;
			} else /* PUT */ {
				// remove outdated file
				CacheRemove(P);
			}
			return true;
		}
		// allocate memory for new File Node
		File_Node *FN = (File_Node *)malloc(sizeof(File_Node));
		if (C->nfiles == NONE) {
			// first file
			C->start = FN;
		}
		strcpy(FN->name, R->addr);
		FN->data = (char *)malloc(R->length);
		FN->ptr = FN->data;
		CacheInsert(FN);
	}
	return false;
}

/* CacheDataIn copies data from a buffer into the most recent File Node */
void CacheDataIn(char *data, int count) {
	// only when cache enabled
	if (cache_enable) {
		memcpy(C->start->ptr, data, count);
		C->start->ptr += count;
	}
}

/* CacheDataOut copies data from File Node P into a buffer */
void CacheDataOut(char *data, int count) {
	// only when cache enabled
	if (cache_enable) {
		memcpy(data, P->ptr, count);
		P->ptr += count;
	}
}

/*** Local Functions ******************************************************/

/* CacheSearch checks to see if a particular file is in the File Cache
 * Returns a pointer to the File_Node if found and NULL otherwise */
File_Node * CacheSearch(char *filename) {
	File_Node *FN = C->start;
	for (int i = 0; i < C->nfiles; i++, FN = FN->next) {
		if (strcmp(FN->name, filename) == 0) {
			return FN;
		}
	}
	return NULL;
}

/* CacheRemove assumes that the File Node is in the File Cache
 * and safely removes then frees all allocated data */
void CacheRemove(File_Node *FN) {
	if (C->start == FN) {
		// if FN is start move start
		C->start = C->start->next;
	}
	// temporary File Node pointer set to node behind FN
	File_Node *T = FN;
	for (int i = 0; i < C->nfiles - 1; i++, T = T->next);
	// cut off FN
	T->next = FN->next;
	// free allocated data
	free(FN->data);
	free(FN);
	C->nfiles--;
}

/* CacheInsert safely inserts a new File Node into the File Cache
 * If the cache is full the oldest file will be removed */
void CacheInsert(File_Node *FN) {
	FN->next = C->start;
	if (C->nfiles != NONE) {
		// temporary File Node pointer set to node behind FN
		File_Node *T = C->start;
		for (int i = 0; i < C->nfiles - 1; i++, T = T->next);
		// last node points to new File Node
		T->next = FN;
		if (C->nfiles == MAX) {
		// nfile set to MAX+1 temporarily in order to remove correctly
			C->nfiles++;
			CacheRemove(T);
		}
	}
	// set start to new File Node
	C->start = FN;
	if (C->nfiles != MAX) {
		C->nfiles++;
	}
}
