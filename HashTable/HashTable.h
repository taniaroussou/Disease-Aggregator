#pragma once
#include "HashTypes.h"
//#include "../BinaryHeap/BinaryHeap.h"
//#define HASH_SIZE 10

typedef struct entry{
    char* key;
    HTItem item;
    struct entry *next;
} Entry;

typedef struct bucket{
    Entry *head;
    int bucketSize;
    int current_nodes;
    int maxNodes;
    struct bucket *next;
} Bucket;

typedef struct {
    int size;       
    int current_entries;
    Bucket **table;
} HTHash;

typedef void (*Visit)(HTHash*,char*,HTItem);
typedef void (*VisitFile)(HTHash*,char*,HTItem,FILE*);
typedef void (*Edit)(HTHash*,HTItem);
typedef void (*Visit2)(HTHash*,char*,HTItem,int,int,time_t, void*);
typedef void (*Visit3)(HTHash*,char*,HTItem,time_t,time_t,char*, void *);

HTHash* HTCreate(int , int );
int HTSize(HTHash* );
int HTGet(HTHash* , char* , HTItem** );
HTHash* HTInsert(HTHash* , char* , HTItem );
void HTRemove(HTHash* , char* );
Entry* HTSearch(HTHash* , char* );
void HTVisit(HTHash* , Visit );
void HTVisitFile(HTHash* , VisitFile , FILE*);
//void HTVisitRange(HTHash* , Visit2 , time_t , time_t );
void HTVisitRange(HTHash* , Visit2 , int , int , time_t, void*);
void HTVisitRange2(HTHash* , Visit3 , time_t , time_t , void * , char *);
void HTEdit(HTHash * , Edit );
void HTDestroy(HTHash* );

