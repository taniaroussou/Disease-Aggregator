#pragma once

enum JobType { STATISTICS, QUERY};

struct data {
    int fd;
    enum JobType type;
};

typedef struct data *BufferData;

struct ringBuffer{
    int head;
    int tail;
    int size;
    BufferData data;
};

typedef struct ringBuffer *RingBuffer;

RingBuffer RingBufferCreate(int );
void RingBufferDestroy(RingBuffer );
void RingBufferInsert(RingBuffer , int , JobType );
struct data RingBufferRemove(RingBuffer );
int RingBufferisFull(RingBuffer );
int RingBufferisEmpty(RingBuffer );
void printBuffer(RingBuffer );

