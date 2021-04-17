#ifndef STREAM_H_
#define STREAM_H_

#define BUFFER_SIZE 5
#define SHM_NAME "/shm_stream"

#include <semaphore.h>

struct stream_t{
    char buffer[BUFFER_SIZE];
    int post_pos;
    int get_pos;
    sem_t sem_empty;
    sem_t sem_fill;
    sem_t mutex;
};


#endif