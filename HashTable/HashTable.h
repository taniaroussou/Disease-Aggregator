#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "HashTable.h"

static int Hash(char *K,HTHash *hash){
    int h=0, a=33;
    for (; *K!='\0'; K++)
        h=(a*h + *K) % hash->size;
    return h;
}

HTHash* HTCreate(int HTSize, int bucketSize){
    HTHash* hash_table = (HTHash*)malloc(sizeof(HTHash));    //hash table
    hash_table->table = (Bucket**)malloc(sizeof(Bucket*)*HTSize); 
    for(int i=0; i<HTSize; i++){
        hash_table->table[i] = (Bucket*)malloc(sizeof(Bucket));  
        hash_table->table[i]->head = NULL;
        hash_table->table[i]->next = NULL;
        hash_table->table[i]->current_nodes = 0;
        hash_table->table[i]->bucketSize = bucketSize;
        hash_table->table[i]->maxNodes = (bucketSize - 3*sizeof(int) - sizeof(Entry*) - sizeof(Bucket*) ) / (sizeof(Entry)) ; 
    }
    hash_table->current_entries = 0;
    hash_table->size = HTSize;
    return hash_table;
}

int HTSize(HTHash* hash){
    return hash->current_entries;
}

Entry* HTSearch(HTHash* hash, char* key){
  
    if (hash->current_entries!=0){
        int h = Hash(key,hash);
        Bucket *tmp = hash->table[h];
        while (tmp!=NULL){
            Entry *tmp2 = tmp->head;
            while(tmp2!=NULL){
                if (strcmp(key,tmp2->key)==0)
                    return tmp2;
                tmp2 = tmp2->next;  //next entry in same bucket    
            }
            tmp = tmp->next;    //next bucket
        }
    }
    return NULL;    //hash table is empty or key not found
}

//Successful 1, unsuccessfull 0
int HTGet(HTHash* hash, char* key, HTItem** pitem){
    Entry *tmp = HTSearch(hash,key);
    if (tmp!=NULL){
        *pitem = &(tmp->item);
        return 1;
    }
    else
        return 0;
}

static HTHash* rehash(HTHash *hash){
    HTHash *hash_table = HTCreate(2*hash->size,hash->table[0]->bucketSize);
    Entry *tmp,*tmp2;
    Bucket *tmp3;
    for(int i=0; i<hash->size; i++){
        tmp3 = hash->table[i];
        while(tmp3!=NULL){
            tmp = tmp3->head;
            while(tmp != NULL){
                tmp2 = HTSearch(hash,tmp->key);
                HTInsert(hash_table,tmp2->key,tmp2->item);
                tmp = tmp->next;
            }
            tmp3 = tmp3->next;  //next bucket
        }
    }
    return hash_table;
}


HTHash* HTInsert(HTHash* hash, char* key, HTItem item){

    int h = Hash(key,hash);
    Entry *tmp = HTSearch(hash,key);    //search if key already exists
    if (tmp==NULL){             //hash table is empty or key not found
        Entry *new1 = (Entry*)malloc(sizeof(struct entry));
        Bucket *temp = hash->table[h];
        new1->item = item;
        new1->key = strdup(key);
        new1->next = NULL;
        int inserted = 0;
        do{
            if (temp->current_nodes<temp->maxNodes){       //if bucket has empty space for entry
                if(temp->current_nodes==0)                            //bucket is empty, insert first entry
                    temp->head = new1;
                else{                                                           //insert new1 entry at bottom of list
                    Entry *tmp2 = temp->head;
                    while(tmp2->next!=NULL)
                        tmp2 = tmp2->next;
                    tmp2->next = new1; 
                }
                inserted = 1;
                temp->current_nodes++;
            }
            else{                           //if Bucket is full
                if (temp->next==NULL){      //if Bucket is not pointing to another Bucket
                    Bucket *newBucket = (Bucket*)malloc(sizeof(Bucket));  
                    newBucket->bucketSize = temp->bucketSize;
                    newBucket->current_nodes = 1;
                    newBucket->maxNodes = temp->maxNodes;
                    newBucket->next = NULL;
                    newBucket->head = new1;
                    temp->next = newBucket;
                    inserted = 1;
                }
                else
                    temp = temp->next;      //next Bucket
            }
        }while(inserted!=1);
        hash->current_entries++;
    }
    else{
        return NULL;
    }
    double loadFactor = (double)hash->current_entries/hash->size;
    if (loadFactor>0.75){
        HTHash *temp = hash;
        hash = rehash(hash);
        HTDestroy(temp);    //delete old table
    }

    return hash; 
}

void HTRemove(HTHash *hash, char* key){
    int h = Hash(key,hash);
    Entry *tmp = HTSearch(hash,key);
    if (tmp!=NULL){     //if key exists
        if (hash->table[h]->head==tmp){         //if entry is at the top of the list
            hash->table[h]->head = tmp->next;   //make the second entry the first entry
            free(tmp->key);
            free(tmp);
        }
        else{
            Entry *tmp2 = hash->table[h]->head;
            Entry *prev = tmp2;
            while(tmp2!=tmp){
                prev = tmp2;
                tmp2 = tmp2->next;
            }
            prev->next = tmp2->next;
            free(tmp2->key);
            free(tmp2);
        }
        hash->table[h]->current_nodes--;
        hash->current_entries--;
    }
}

void HTVisit(HTHash *hash , Visit visit){
    Entry *tmp,*tmp2;
    Bucket *temp;
    for(int i=0; i<hash->size; i++){
        temp = hash->table[i];
        while (temp!=NULL){
            if (temp->current_nodes!=0){
                tmp = temp->head;
                while(tmp!=NULL){
                    tmp2 = tmp;
                    tmp = tmp->next;
                    visit(hash,tmp2->key,tmp2->item);
                }
            }
            temp = temp->next;
        }
    }
}

void HTVisitFile(HTHash *hash , VisitFile visit, FILE* myfile){
    Entry *tmp,*tmp2;
    Bucket *temp;
    for(int i=0; i<hash->size; i++){
        temp = hash->table[i];
        while (temp!=NULL){
            if (temp->current_nodes!=0){
                tmp = temp->head;
                while(tmp!=NULL){
                    tmp2 = tmp;
                    tmp = tmp->next;
                    visit(hash,tmp2->key,tmp2->item,myfile);
                }
            }
            temp = temp->next;
        }
    }
}

void HTVisitRange(HTHash *hash , Visit2 visit,int min,int max, time_t date, void *stats){
    Entry *tmp,*tmp2;
    Bucket *temp;
    for(int i=0; i<hash->size; i++){
        temp = hash->table[i];
        while (temp!=NULL){
            if (temp->current_nodes!=0){
                tmp = temp->head;
                while(tmp!=NULL){
                    tmp2 = tmp;
                    tmp = tmp->next;
                    visit(hash,tmp2->key,tmp2->item,min,max,date,stats);
                }
            }
            temp = temp->next;
        }
    }
}

void HTVisitRange2(HTHash* hash, Visit3 visit, time_t date1, time_t date2,void *result,char *country){
    Entry *tmp,*tmp2;
    Bucket *temp;
    for(int i=0; i<hash->size; i++){
        temp = hash->table[i];
        while (temp!=NULL){
            if (temp->current_nodes!=0){
                tmp = temp->head;
                while(tmp!=NULL){
                    tmp2 = tmp;
                    tmp = tmp->next;
                    visit(hash,tmp2->key,tmp2->item,date1,date2,country,result);
                }
            }
            temp = temp->next;
        }
    }
}

void HTEdit(HTHash *hash , Edit edit){
    Entry *tmp,*tmp2;
    Bucket *temp;
    for(int i=0; i<hash->size; i++){
        temp = hash->table[i];
        while (temp!=NULL){
            if (temp->current_nodes!=0){
                tmp = temp->head;
                while(tmp!=NULL){
                    tmp2 = tmp;
                    tmp = tmp->next;
                    edit(hash,tmp2->item);
                }
            }
            temp = temp->next;
        }
    }
}

void HTDestroy(HTHash *hash){
    Entry *tmp,*tmp2;
    Bucket *temp,*temp2;
    for(int i=0; i<hash->size; i++){
        temp = hash->table[i];
        while (temp!=NULL){
            temp2 = temp;
            if (temp->current_nodes!=0){
                tmp = temp->head;
                while(tmp!=NULL){
                    tmp2 = tmp;
                    tmp = tmp->next;
                    free(tmp2->key);
                    free(tmp2);     //free entry
                }
            }
            temp = temp->next;
            free(temp2);     //free bucket
        }
    }
    free(hash->table);
    free(hash);
}
