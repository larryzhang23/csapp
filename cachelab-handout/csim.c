#include "cachelab.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

// maximum number of characters in each line of the .trace files
#define MAX_COMMAND_CHAR 1000

// maximum number of characters for the address in the line of .trace files
#define MAX_ADDR_CHAR 17

static unsigned long ts;

typedef struct {
    bool valid;
    unsigned long tagBitVal;
    unsigned long timestamp;
} Line;

typedef struct {
    Line *lines;
    unsigned lineNum;
} Set;

typedef struct {
    Set *sets;
    unsigned setNum;
} Cache;

typedef struct {
    char command;
    unsigned long addr;
} CacheRef;

typedef struct {
    int hits;
    int misses;
    int evicts;
} OpsResult;

/* Get the set index */
unsigned long getSetIndex(unsigned long addr, unsigned blockBits, unsigned setBits);

/* get the tag bits equivalent decimal value */
unsigned long getTagBitsVal(unsigned long addr, unsigned blockBits, unsigned setBits);

/* read the command from string to the cache ref stuct */
void getCommand(char *command, unsigned commandLength, CacheRef *cacheRef);

/* create cache based on the number of set bits and associativity */
Cache *createCache(unsigned setBits, unsigned associativity);

/* free cache occupied heap space */
void freeCache(Cache *cache);

/* simulate real cache operation */
void operateCache(char cmd, unsigned long setIndex, unsigned long tagBitsVal, Cache *cache, OpsResult *ops);

/* check if cache is initialized correctly */
void checkCacheInit(Cache *cache);

int main(int argc, char *argv[])
{   
    // read arguments
    int setBits = -1;
    int associativity = -1;
    int blockBits = -1;
    char *traceFile = NULL;
    bool verbal = false;
    char *commandUsage = "Usage: ./csim [-hv] -s <num> -E <num> -b <num> -t <file>\n"
                         "Options:\n"
                            "-h         Print this help message.\n"
                            "-v         Optional verbose flag.\n"
                            "-s <num>   Number of set index bits.\n"
                            "-E <num>   Number of lines per set.\n"
                            "-b <num>   Number of block offset bits.\n"
                            "-t <file>  Trace file.";

    // TODO: add error handling to atoi function
    // TODO: use getopt to parse param
    while (--argc > 0) {
        char *arg = *++argv;
        if (strcmp(arg, "-h") == 0) {
            printf("%s\n", commandUsage);
            exit(0);
        } else if (strcmp(arg, "-s") == 0) {
            argc--;
            setBits = atoi(*++argv);
        } else if (strcmp(arg, "-E") == 0) {
            argc--;
            associativity = atoi(*++argv);
        } else if (strcmp(arg, "-b") == 0) {
            argc--;
            blockBits = atoi(*++argv);
        } else if (strcmp(arg, "-t") == 0) {
            argc--;
            traceFile = *++argv;
        } else if (strcmp(arg, "-v") == 0) {
            verbal = true;
        }
    }

    // TODO: add more error handling checking logic for the results like setBits > 0
    if (setBits == -1 || associativity == -1 || blockBits == -1 || traceFile == NULL) {
        printf("Wrong command! %s\n", commandUsage);
        exit(EXIT_FAILURE);
    }
    // printf("setBits: %d, asso: %d, blocBits: %d, traceFile: %s\n", setBits, associativity, blockBits, traceFile);

    // build the cache 
    Cache *cache = createCache(setBits, associativity);
    // checkCacheInit(cache);

    if (cache == NULL) {
        printf("Alloc cache mem fails.");
        exit(EXIT_FAILURE);
    }

    // open file
    FILE *fp;
    if ((fp = fopen(traceFile, "r")) == NULL) {
        printf("Can't open the tracefiles: %s\n", traceFile);
        exit(EXIT_FAILURE);
    }

    // start cache simulation
    char command[MAX_COMMAND_CHAR];
    CacheRef ref;
    int totalHits, totalMisses, totalEvicts;
    totalHits = totalMisses = totalEvicts = 0;
    while (fgets(command, MAX_COMMAND_CHAR, fp) != NULL) {
        unsigned charNum = strlen(command);
        // replace the newline character
        if (command[charNum - 1] == '\n') {
            command[charNum - 1] = '\0';
            charNum--;
        }
        // skip the instruction loading command
        if (command[0] != ' ') 
            continue;
        // parse the command
        getCommand(command, charNum, &ref);
        // read the set index and tag bits
        unsigned long setIndex = getSetIndex(ref.addr, blockBits, setBits);
        unsigned long tagBitsVal = getTagBitsVal(ref.addr, blockBits, setBits);
        // printf("command: %s, setIndex: %lu, tagBitsVal: %lu\n", command, setIndex, tagBitsVal);
        // operate the cache
        OpsResult opsResult = {0, 0, 0};
        operateCache(ref.command, setIndex, tagBitsVal, cache, &opsResult);
        totalHits += opsResult.hits;
        totalMisses += opsResult.misses;
        totalEvicts += opsResult.evicts;

        // if it is verbal mode, print the result
        if (verbal) {
            switch (ref.command) {
                case 'L':
                case 'S':
                    if (opsResult.hits > 0) 
                        printf("%s hit\n", command);
                    else if (opsResult.evicts == 0)
                        printf("%s miss\n", command);
                    else 
                        printf("%s miss eviction\n", command);
                    break;
                default:
                    if (opsResult.hits == 2) 
                        printf("%s hit hit\n", command);
                    else if (opsResult.evicts == 0)
                        printf("%s miss hit\n", command);
                    else
                        printf("%s miss eviction hit\n", command);
            }
        }
    }

    // error handling for reading files
    if (ferror(fp) != 0) {
        printf("Encounting error while reading the files");
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    // close file and free cache
    fclose(fp);
    freeCache(cache);

    // print all hits, misses and evicts for autograder to catch
    printSummary(totalHits, totalMisses, totalEvicts);
    return 0;
}

unsigned long getSetIndex(unsigned long addr, unsigned blockBits, unsigned setBits) {
    unsigned indexBits = blockBits + setBits;
    unsigned long mask = (1 << indexBits) - (1 << blockBits);
    // printf("mask: 0x%lx, tagBitsVal: %lu\n", mask, (addr & mask) >> blockBits);
    return (addr & mask) >> blockBits;
}

unsigned long getTagBitsVal(unsigned long addr, unsigned blockBits, unsigned setBits) {
    unsigned indexBits = blockBits + setBits;
    unsigned long mask = ~((1 << indexBits) - 1);
    // printf("mask: %lx, tagBits: %lx, shit: %.8lx\n", mask, (addr & mask) >> indexBits, (unsigned long)((1 << indexBits) - 1));
    return (addr & mask) >> indexBits;
}

void getCommand(char *command, unsigned commandLength, CacheRef *cacheRef) {
    if (command == NULL)
        return;
    // printf("command: %s\t", command);
    char addr[MAX_ADDR_CHAR];
    int idx = 0;
    char *end = command + commandLength;
    for (; command < end; command++) {
        if (*command == ' ')
            continue;
        if (isalpha(*command) && isupper(*command) && (command + 1 == end || (*(command + 1) == ' '))) 
            cacheRef->command = *command;
        else if (isalpha(*command) || isdigit(*command)) {
            addr[idx++] = *command;
        } else if (*command == ',')
            break;
    }
    if (idx >= MAX_ADDR_CHAR)
        return;
    addr[idx] = '\0';
    // printf("idx: %d addr: %s\n", idx, addr);
    cacheRef->addr = strtoul(addr, NULL, 16);
}

Cache *createCache(unsigned setBits, unsigned associativity) {
    Cache *cache = (Cache *) malloc(sizeof(Cache));
    if (cache == NULL)
        return NULL;
    unsigned setNum = 1 << setBits;
    Set *sets = (Set *) malloc(setNum * sizeof(Set));
    if (sets == NULL) {
        free(cache);
        return NULL;
    }
    cache->setNum = setNum;
    cache->sets = sets;
    
    bool allocError = false;
    for (Set *setEnd = sets + setNum; sets < setEnd; sets++) {
        Line *lines = (Line *) malloc(associativity * sizeof(Line));
        if (lines == NULL) {
            allocError = true;
            break;
        }
        sets->lineNum = associativity;
        sets->lines = lines;
        for (Line *lineEnd = lines + associativity; lines < lineEnd; lines++) {
           lines->tagBitVal = 0;
           lines->timestamp = 0;
           lines->valid = false;
        }
    }
    if (allocError) {
        freeCache(cache);
        return NULL;
    }
    return cache;
}

void freeCache(Cache *cache) {
    for (Set *sets = cache->sets, *setsEnd = sets + cache->setNum; sets < setsEnd; sets++) {
        free(sets->lines);
        sets->lines = NULL;
    }
    free(cache->sets);
    free(cache);
}

void operateCache(char cmd, unsigned long setIndex, unsigned long tagBitsVal, Cache *cache, OpsResult *ops) {
    if (cmd == 'M')
        ops->hits = 1;
    // printf("setIndex: %lu\n", setIndex);
    Set *set = cache->sets + setIndex;
    // printf("setNum: %u lineNum: %u\n", cache->setNum, set->lineNum);
    for (Line *line = set->lines, *lineEnd = line + set->lineNum; line < lineEnd; line++) {
        if (line->valid && line->tagBitVal == tagBitsVal) {
            line->timestamp = ts++;
            ops->hits++;
            return;
        }
    }

    ops->misses = 1;

    unsigned long minTs = ~0;
    Line *evictLine = NULL;

    for (Line *line = set->lines, *lineEnd = line + set->lineNum; line < lineEnd; line++) {
        if (!line->valid) {
            line->timestamp = ts++;
            line->valid = true;
            line->tagBitVal = tagBitsVal;
            return;
        } else if (line->timestamp < minTs) {
            evictLine = line;
            minTs = line->timestamp;
        }
    }
    ops->evicts = 1;
    evictLine->tagBitVal = tagBitsVal;
    evictLine->timestamp = ts++;
    return;
}


void checkCacheInit(Cache *cache) {
    Set *sets = cache->sets;
    int count = 0;
    printf("set number: %d\n", cache->setNum);
    for (Set *setEnd = sets + cache->setNum; sets < setEnd; sets++) {
        Line *lines = sets->lines;
        printf("%d: lineNum: %u, lineHead: %lx\n", count, sets->lineNum, (unsigned long) sets->lines);
       
        for (Line *lineEnd = lines + sets->lineNum; lines < lineEnd; lines++) {
           printf("\tvalid: %d, tagVal: %lu, ts: %lu\n", (int)lines->valid, lines->tagBitVal, lines->timestamp);
        }
        count++;
    }
}