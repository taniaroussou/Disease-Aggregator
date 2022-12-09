#include <stdio.h>
#include <stdlib.h>
#include "RingBuffer.h"

RingBuffer RingBufferCreate(int size){
    RingBuffer newBuffer = (RingBuffer)malloc(sizeof(struct ringBuffer));
    if (newBuffer == NULL){
        printf("Error, not enough memory\n");
        return NULL;
    }

    newBuffer->data = (BufferData)malloc(sizeof(struct data)*size);
    if (newBuffer->data == NULL){
        printf("Error, not enough memory\n");
        return NULL;
    }
    newBuffer->size = size;
    newBuffer->head = newBuffer->tail = -1;
    return newBuffer;
}

int RingBufferisFull(RingBuffer buffer){
    return ( (buffer->head == 0 && buffer->tail == buffer->size-1) || (buffer->tail == (buffer->head-1)%(buffer->size-1)) );
}

int RingBufferisEmpty(RingBuffer buffer){
    return (buffer->head == -1);
}

void RingBufferDestroy(RingBuffer buffer){
    if (buffer != NULL){
        free(buffer->data);
        free(buffer);
    }
}

void RingBufferInsert(RingBuffer buffer, int data, JobType type){
    if (buffer->head == -1)    //1st element
        buffer->head = buffer->tail = 0;
    else if (buffer->tail == buffer->size-1 && buffer->head != 0)
        buffer->tail = 0;
    else 
        buffer->tail++;

    buffer->data[buffer->tail].fd = data;   
    buffer->data[buffer->tail].type = type;   
}

struct data RingBufferRemove(RingBuffer buffer){
    struct data tmp = buffer->data[buffer->head];
    buffer->data[buffer->head].fd = -1;
    if (buffer->head == buffer->tail)
        buffer->head = buffer->tail = -1;
    else if (buffer->head == buffer->size-1)
        buffer->head = 0;
    else
        buffer->head++;
    return tmp;
}

void printBuffer(RingBuffer buffer){
    printf("head = %d\n", buffer->head);
    for(int i=buffer->head; i<buffer->tail; i++){
        printf("data[%d] = %d\n", i, buffer->data[buffer->head].fd);
    }
    printf("tail = %d\n", buffer->tail);
}
