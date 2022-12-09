#include "ListTypes.h"

struct list_node {
    LItem item;
    struct list_node *next;
};

typedef struct list_node* LNode;
typedef LNode LList;


LList LCreate();
void LDestroy(LList );
int LLength(LList );
LItem LGetItem(LList , LNode );
LNode LInsertFirst(LList , LItem );
int LFindSum(LList , char* );
int LSumAge(LList , int , int );
int LSumAge2(LList , int , int ,char* );
int LSumCountry(LList , char *, time_t, time_t);
int LSumCountry2(LList , char *);
int LSumDishargeCountry(LList , char *,time_t , time_t );