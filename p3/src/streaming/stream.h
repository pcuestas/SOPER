#ifndef STREAM_H_
#define STREAM_H_

#include <semaphore.h>

#define BUFFER_SIZE 5
#define SHM_NAME "/shm_stream"
#define MQ_SERVER "/mq_server"
#define MQ_CLIENT "/mq_client"
#define MSG_SIZE 4


struct stream_t{
    char buffer[BUFFER_SIZE];
    int post_pos;
    int get_pos;
    sem_t sem_empty;
    sem_t sem_fill;
    sem_t mutex;
};


#endif