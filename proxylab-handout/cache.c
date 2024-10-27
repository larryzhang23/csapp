#include "cache.h"

static void free_cell(Cell *ptr, Cache *cache);

static void print_cache(Cache *cache);

void cache_init(Cache *cache) {
    cache->head = NULL;
    cache->size = 0;
    cache->rthread_n = 0;
    Sem_init(&cache->rd_mutex, 0, 1);
    Sem_init(&cache->rw_lock, 0, 1);
}

ssize_t read_cache(Cache *cache, char *url, char *buf, size_t n) {
    size_t cpy_sz = 0;

    P(&cache->rd_mutex);
    if (cache->rthread_n == 0)
        P(&cache->rw_lock);
    cache->rthread_n++;
    V(&cache->rd_mutex);

    Cell *ptr = cache->head;
    while (ptr != NULL) {
        if (strcmp(ptr->url, url) == 0) 
            break;
        ptr = ptr->next;
    }
    if (ptr != NULL) {
        cpy_sz = (n >= ptr->size ? ptr->size: n);
        strncpy(buf, ptr->object, cpy_sz);
        ptr->t = time(NULL);
    }

    P(&cache->rd_mutex);
    cache->rthread_n--;
    printf("%s\ncache: ", "try to read cache");
    print_cache(cache);
    if (cache->rthread_n == 0)
        V(&cache->rw_lock);
    V(&cache->rd_mutex);
   
    return cpy_sz;
}

ssize_t write_cache(Cache *cache, char *url, char *buf, size_t n) {
    P(&cache->rw_lock);

    Cell *ptr = cache->head;
    Cell *evict = NULL;
    time_t t;

    while (ptr != NULL) {
        if (strcmp(ptr->url, url) == 0) {
            free_cell(ptr, cache);
            break;
        }
        ptr = ptr->next;
    }

    while (cache->size + n > MAX_CACHE_SIZE) {
        t = __INT64_MAX__;
        ptr = cache->head;
        while (ptr != NULL) {
            if (t > ptr->t) {
                evict = ptr;
                t = ptr->t;
            }
            ptr = ptr->next;
        }
        free_cell(evict, cache);
    }

    ptr = (Cell *) malloc(sizeof(Cell));
    ptr->url = (char *) malloc(strlen(url) + 1);
    strcpy(ptr->url, url);
    ptr->object = (char *) malloc(n);
    ptr->size = n;
    cache->size += n;
    strncpy(ptr->object, buf, n);
    ptr->t = time(NULL);
    ptr->prev = NULL;
    if (cache->head != NULL)
        cache->head->prev = ptr;
    ptr->next = cache->head;
    cache->head = ptr;

    printf("%s\ncache: ", "Try to write cache");
    print_cache(cache);
    V(&cache->rw_lock);
    
    return n;
}


static void free_cell(Cell *ptr, Cache *cache) {
    if (ptr == NULL)
        return;
    free(ptr->url);
    free(ptr->object);
    cache->size -= ptr->size;
    if (ptr->prev != NULL)
        ptr->prev->next = ptr->next;
    if (ptr->next != NULL)
        ptr->next->prev = ptr->prev;
    free(ptr);
}

static void print_cache(Cache *cache) {
    Cell *ptr = cache->head;
    while (ptr != NULL) {
        printf("%s:[size: %d bytes, t: %ld]->", ptr->url, ptr->size, ptr->t);
        ptr = ptr->next;
    }
    printf("%c", '\n');
}
