#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ListInterface.h"
#include "../Patient.h"

LList LCreate(){
    LNode new_node = (LNode)malloc(sizeof(struct list_node));   //create dummy node to represent the list
    if (new_node==NULL){
        printf("Error, not enough memory\n");
        return NULL;
    }
    new_node->next = NULL;
    return new_node;
}

void LDestroy(LList list){
    LNode tmp = list, tmp2;
    while(tmp!=NULL){
        tmp2 = tmp->next;
        free(tmp);
        tmp = tmp2;
    }
}

int LLength(LList list){
    int len = 0;
    LNode tmp = list->next; //skip dummy node
    while(tmp!=NULL){
        len++;
        tmp = tmp->next;
    }
    return len;
}

LItem LGetItem(LList list, LNode node){
    if (node!=NULL)
        return node->item;
    else
        return NULL;
}

LNode LInsertFirst(LList list, LItem item){
    LNode new_node = (LNode)malloc(sizeof(struct list_node));
    if (new_node==NULL){
        printf("Error, not enough memory\n");
        return NULL;
    }
    new_node->item = item;
    new_node->next = list->next;
    list->next = new_node;      //skip dummy
    return new_node;
}


int LFindSum(LList list, char* key){
    int sum = 0;
    LNode temp = list->next;    //skip dummy
    while(temp!=NULL){
        Patient *tmp_patient = (Patient*)temp->item;
        char* tempkey = &((tmp_patient->country)[0]);
        if(strcmp(tempkey, key) == 0)
            sum++;
        temp = temp->next;
    }
    return sum;
}

int LSumAge(LList list, int min, int max){
    int sum = 0;
    LNode temp = list->next;    //skip dummy
    while(temp!=NULL){
        Patient *tmp_patient = (Patient*)temp->item;
        int age = tmp_patient->age;
        if(age>=min && age<=max)
            sum++;
        temp = temp->next;
    }
    return sum;
}

int LSumAge2(LList list, int min, int max, char *disease){
    int sum = 0;
    LNode temp = list->next;    //skip dummy
    while(temp!=NULL){
        Patient *tmp_patient = (Patient*)temp->item;
        int age = tmp_patient->age;
        
        char * diseaseid = &((tmp_patient->disease)[0]);
        if(age>=min && age<=max && strcmp(disease,diseaseid)==0)
            sum++;
        temp = temp->next;
    }
    return sum;
}

int LSumCountry(LList list, char *name,time_t date1, time_t date2){
    int sum = 0;
    LNode temp = list->next;    //skip dummy
    while(temp!=NULL){
        Patient *tmp_patient = (Patient*)temp->item;
        char * diseaseid = &((tmp_patient->disease)[0]);
        if(strcmp(name,diseaseid)==0 && tmp_patient->exitDate>=date1 && tmp_patient->exitDate<=date2)
            sum++;
        temp = temp->next;
    }
    return sum;
}

int LSumCountry2(LList list, char *name){
    int sum = 0;
    LNode temp = list->next;    //skip dummy
    while(temp!=NULL){
        Patient *tmp_patient = (Patient*)temp->item;
        char * diseaseid = &((tmp_patient->disease)[0]);
        if(strcmp(name,diseaseid)==0)
            sum++;
        temp = temp->next;
    }
    
    return sum;
}

int LSumDishargeCountry(LList list, char *name,time_t date1, time_t date2){
    int sum = 0;
    LNode temp = list->next;    //skip dummy
    while(temp!=NULL){
        Patient *tmp_patient = (Patient*)temp->item;
        char* tempkey = &((tmp_patient->country)[0]);
        if(strcmp(name,tempkey)==0 && tmp_patient->exitDate>=date1 && tmp_patient->exitDate<=date2)
            sum++;
        temp = temp->next;
    }
    return sum;
}
