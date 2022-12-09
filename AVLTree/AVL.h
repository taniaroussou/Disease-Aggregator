#pragma once
#include "AVLTypes.h"

struct node {
    time_t date;    //key
    AVLItem item;
    int height;     
    struct node *left_child;
    struct node *right_child;
};

struct tree {
    struct node *root;
    int size;
};

typedef struct node *AVLNode;
typedef struct tree *AVLTree;

AVLTree AVLCreate();
AVLTree AVLInsert(AVLTree ,AVLItem ,time_t );
void AVLDestroy(AVLTree );
int AVLCountRange(AVLTree ,int , int , time_t );
int AVLCountRangeKey(AVLTree ,time_t , time_t ,char* );
int AVLCount(AVLTree , int , int );
int AVLCountCountry(AVLTree ,time_t, time_t, char *);
int AVLCountCountry2(AVLTree ,time_t, time_t, char *);
int AVLCountRange2(AVLTree ,time_t , time_t );
int AVLCountRangeKey2(AVLTree ,time_t , time_t ,char* );
int AVLCountRange3(AVLTree ,int , int , time_t , time_t , char * );