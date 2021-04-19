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
#include <errno.h>
#include <mqueue.h>

#include "stream.h"



void read_fromFile(int fd, Stream_t *shm_struct,mqd_t queue){
    int nBytes = 1;
    int val, s, i;
    struct timespec ts;

    do{
        
        val=_receive_message(queue); /*receive exit message*/
        if(val==0){
            return;
        }

        /*if((_sem_timedwait(&(shm_struct->sem_empty)))==-1){
            printf("Espera superior a 2 segundos en server");
            continue;
        }*/

        _sem_timedwait(&(shm_struct->sem_empty));
        sem_wait(&(shm_struct->sem_mutex));
        do{
            i=0;
            nBytes = read(fd, &(shm_struct->buffer[shm_struct->post_pos]), sizeof(char));
            printf("%i ",i);
            i++;
        }while(nBytes==-1);
        
        printf("Server escribe %i \n", nBytes);
        shm_struct->post_pos = (shm_struct->post_pos + 1) % BUFFER_SIZE;

        sem_post(&(shm_struct->sem_mutex));
        sem_post((&(shm_struct->sem_fill)));
    } while (nBytes != 0);

    _sem_timedwait(&(shm_struct->sem_empty));
    sem_wait(&(shm_struct->sem_mutex)); //este wait deberia ser nomal?
    shm_struct->buffer[shm_struct->post_pos] = '\0';
    sem_post(&(shm_struct->sem_mutex));
    sem_post((&(shm_struct->sem_fill)));
}


int main(int argc, char *args[]){
    int fd;
    Stream_t *shm_struct = NULL;
    mqd_t queue;
    
    /*Abrir el fichero de lectura*/
    if ((fd = open(args[1], O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1){
        perror("open");
        exit(EXIT_FAILURE);
    }

    queue = open_message_queues(MQ_SERVER, O_RDONLY);

    shm_struct = open_shmSegment(args[2]); // se puede poner SHM_NAME

    read_fromFile(fd, shm_struct,queue);
    close(fd);
    mq_close(queue);

    return 0;
}