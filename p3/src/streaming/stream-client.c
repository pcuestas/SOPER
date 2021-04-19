#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <mqueue.h>
#include <string.h>

#include "stream.h"

#define MSG__GET 1
#define MSG__EXIT 3
#define MSG__OTHER 0

/**
 * @brief accede a la estructura de datos compartidos para 
 * leer un caracter del buffer apuntado por get_pos. Y 
 * actualiza el valor de get_pos en caso de que no se lea 
 * '\0'. Pone el valor de *err a 1 en caso de error.
 * 
 * @param stream_shm  estructura de los datos compartidos
 * @param fd_output fichero en el que se escribe el output
 * @param err vale 0, se establece su valor a 1 en caso de error
 * @return el carater leído
 */
char st_shm_get_element(struct stream_t *stream_shm, int fd_output, int *err)
{
    char c = stream_shm->buffer[stream_shm->get_pos];

    if (c != '\0')
    {
        if (write(fd_output, &c, sizeof(c)) == -1)
        {
            perror("write");
            (*err) = 1;
        }
        (stream_shm->get_pos) = (stream_shm->get_pos + 1) % BUFFER_SIZE;
    }
    return c;
}


int main(int argc, char *argv[]){
    struct stream_t *stream_shm;
    int fd_output, err = 0, time_out = 0, msg;
    char c;
    struct timespec ts;
    mqd_t queue;

    if (argc != 2){
        fprintf(stderr, "Uso:\t%s <output_file>", argv[0]);
        exit(EXIT_FAILURE);
    }

    fd_output = open(argv[1], O_CREAT | O_TRUNC | O_WRONLY,
                     S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
    if(fd_output == -1)
    {
        perror("open output file");
        exit(EXIT_FAILURE);
    }

    /*abrir la cola de mensajes*/
    queue = mq_open(MQ_CLIENT, O_RDONLY);
    if(queue == (mqd_t)-1)
    {
        perror("mq_open");
        close(fd_output);
        exit(EXIT_FAILURE);
    }

    /*mapeo de la memoria compartida*/
    stream_shm = st_shm_open();
    if(stream_shm == MAP_FAILED)
    {
        close(fd_output);
        mq_close(queue);
        exit(EXIT_FAILURE);
    }

    /*consumidor*/
    do
    {
        if(mq_receive(queue, (char*)(&msg), sizeof(msg), NULL) == -1)
        {
            perror("mq_receive");
            err = 1;
            break;    
        }

        if(msg == MSG__EXIT)
            break;

        if(st_timed_wait(&(stream_shm->sem_fill), &ts, 2, &err, &time_out) == EXIT_FAILURE)
            break;
        else if (time_out) /*Si la espera es superior a 2 segundos, se desecha la operación*/
            continue;
        
        if(st_timed_wait(&(stream_shm->mutex), &ts, 2, &err, &time_out) == EXIT_FAILURE)
            break;
        else if(time_out)
        {
            sem_post(&(stream_shm->sem_fill));
            continue;
        }

        c = st_shm_get_element(stream_shm, fd_output, &err);

        sem_post(&(stream_shm->mutex));
        sem_post(&(stream_shm->sem_empty));
    } while (c != '\0' && !err);

    close(fd_output);

    if(c == '\0' && !err)
        st_ingore_until_exit(queue, &err);

    mq_close(queue);
    munmap (stream_shm, sizeof(struct stream_t)) ;

    exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
}