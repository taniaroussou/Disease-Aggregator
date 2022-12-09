#include "SymbolTable.h"
#define HASH_MULTIPLIER 65599
#define SIZE 509

struct SymTable_S{
	struct node *Table[SIZE];
	unsigned int numOfItems;
};

struct node{
	char *key;
	void *value;
	struct node *next;
};

/*	HASH FUNCTION: */
static unsigned int SymTable_hash(const char *pcKey){
	unsigned int i;
	unsigned int hashValue = 0U;
	i = 0U;
	while(pcKey[i]){
		hashValue = hashValue * HASH_MULTIPLIER + pcKey[i];
		i++;
	}
	return (hashValue % SIZE);
}


/* Creates empty SymTable_T

Returns: SymTable_T */
SymTable_T SymTable_new(void){
	int i;
	struct SymTable_S *ptr = (struct SymTable_S *)malloc(sizeof(struct SymTable_S));
	for(i=0; i<SIZE; i++){	/* set all element of hash array to NULL */
		ptr->Table[i] = NULL;
	}
	ptr->numOfItems = 0; 
	return ptr;
}


/* Frees all memory allocated by the argument

Arg: SymTable_T*/
void SymTable_free(SymTable_T oSymTable){
	int i;
	struct node *ptr, *prevPtr;
	if(!oSymTable)
		return;
	for(i=0; i<SIZE; i++){	/* Loop all element of the array */
		if(oSymTable->Table[i]){
			ptr = oSymTable->Table[i];
			prevPtr = ptr;
			while(ptr){		/* Loop all elements of list */
				ptr = ptr->next;
				free(prevPtr->key);
				free(prevPtr);	/* Free element */
				prevPtr = ptr;
			}
		}
	}
	free(oSymTable);
}


/* Returns amount of elements currently contained

Arg: SymTable_T
Returns: amount of elements of SymTable_T */
unsigned int SymTable_getLength(SymTable_T oSymTable){
	assert(oSymTable);
	return (oSymTable->numOfItems);
}


/* Adds element to oSymTable with key = pcKey and value = pvValue
pcKey is coppied, pvValue no.
Args: SymTable_T, const pointer to string, const void pointer
Returns: 1 if element with pcKey doesnt exist, else 0 */
int SymTable_put(SymTable_T oSymTable, const char *pcKey, const void *pvValue){
	struct node *ptr;
	char *tempString;
	assert(oSymTable);
	assert(pcKey);
	if(SymTable_contains(oSymTable, pcKey))
		return 0;
	tempString = (char*)malloc(strlen(pcKey) + 1); 
	strcpy(tempString, pcKey);
	ptr = (struct node*)malloc(sizeof(struct node));
	ptr->key = tempString;
	ptr->value = (void*)pvValue;
	ptr->next = oSymTable->Table[SymTable_hash(pcKey)];
	oSymTable->Table[SymTable_hash(pcKey)] = ptr;
	oSymTable->numOfItems++;
	return 1;
}

/* Removes and frees the element with pcKey

Args: SymTable_T, const pointer to string
Returns: 1 if an element was removed, else 0 */
int SymTable_remove(SymTable_T oSymTable, const char *pcKey){
	struct node *ptr, *prevPtr;
	assert(oSymTable);
	assert(pcKey);
	ptr = oSymTable->Table[SymTable_hash(pcKey)];
	if(!ptr)
		return 0;
	if(strcmp(pcKey, ptr->key) == 0){
		oSymTable->Table[SymTable_hash(pcKey)] = ptr->next;
		free(ptr->key);
		free(ptr);
		oSymTable->numOfItems--;
		return 1;
	}
	prevPtr = ptr;
	ptr = ptr->next;
	while(ptr){
		if(strcmp(pcKey, ptr->key) == 0){
			prevPtr->next = ptr->next;
			free(ptr->key);
			free(ptr);
			oSymTable->numOfItems--;
			return 1;
		}
		prevPtr = ptr;
		ptr = ptr->next;
	}
	return 0;
}

/* Checks if an element with key = pcKey is contained 

Args: SymTable_T, const pointer to string
Returns: 1 if the element is contained, else 0 */
int SymTable_contains(SymTable_T oSymTable, const char *pcKey){
	struct node *ptr;
	assert(oSymTable);
	assert(pcKey);
	ptr = oSymTable->Table[SymTable_hash(pcKey)];
	while(ptr){
		if(strcmp(pcKey, ptr->key) == 0)
			return 1;
		ptr = ptr->next;
	}
	return 0;
}

/* Returns the value of the element with key = pcKey

Args: SymTable_T, const pointer to string, 
Returns: void pointer to pvValue of element OR NULL if not contained */
void *SymTable_get(SymTable_T oSymTable, const char *pcKey){
	struct node *ptr;
	assert(oSymTable);
	assert(pcKey);
	ptr = oSymTable->Table[SymTable_hash(pcKey)];
	while(ptr){
		if(strcmp(pcKey, ptr->key) == 0)
			return (ptr->value);
		ptr = ptr->next;
	}
	return NULL;
}

/* Applies pfApply to all elements of oSymTable

Args: SymTable_T, pointer to function pfApply, const void pointer to pvExtra  */
void  SymTable_map(SymTable_T oSymTable, void (*pfApply)(const char *pcKey,
 void *pvValue, void *pvExtra), const void *pvExtra){
 	int i;
	struct node *ptr;
	assert(oSymTable);
	assert(pfApply);
	for(i=0; i<SIZE; i++){
		ptr = oSymTable->Table[i];
		while(ptr){
			pfApply((const char*)ptr->key, ptr->value, (void*)pvExtra);
			ptr = ptr->next;
		}
	}
}
