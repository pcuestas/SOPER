#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <mqueue.h>
#include "stream.h"


void write_toFile(int fd, Stream_t *shm_struct,mqd_t queue)
{
    int nBytes,val;
    char c;

    do{

        val = _receive_message(queue); /*receive exit message*/
        printf("Client ha recibido %i \n", val);
        if (val == 0){
            printf("Me salgo client");
            return;
        }

        sem_wait(&(shm_struct->sem_fill));
        sem_wait(&(shm_struct->sem_mutex));
        c = shm_struct->buffer[shm_struct->get_pos];
        printf("Client escribe %c \n",c);
        write(fd,&c,sizeof(char));
        shm_struct->get_pos = (shm_struct->get_pos + 1) % BUFFER_SIZE;
        sem_post(&(shm_struct->sem_mutex));
        sem_post(&(shm_struct->sem_empty));

    } while (c != '\0');
}


int main(int argc, char *args[]){

    int fd;
    Stream_t *shm_struct = NULL;
    mqd_t queue;

    if ((fd = open(args[1], O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1){
        perror("open");
        exit(EXIT_FAILURE);
    }

    queue = open_message_queues(MQ_CLIENT, O_RDONLY);
    shm_struct = open_shmSegment(args[2]);

    write_toFile(fd, shm_struct,queue);
    close(fd);
    mq_close(queue);

    return 0;
}