#ifndef __CACHE__
#define __CACHE__

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct cell {
    char *url;
    char *object;
    int size;
    struct cell *next;
    struct cell *prev; 
    time_t t;
} Cell;

typedef struct cache {
    Cell *head;
    int size;
    sem_t rd_mutex;
    sem_t rw_lock;
    int rthread_n;
} Cache;

void cache_init(Cache *cache);

ssize_t read_cache(Cache *cache, char *url, char *buf, size_t n);

ssize_t write_cache(Cache *cache, char *url, char *buf, size_t n);

#endif