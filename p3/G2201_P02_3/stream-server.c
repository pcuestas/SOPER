/**
 * @file stream-server.c
 * @author Pablo Cuesta Sierra, Álvaro Zamanillo Sáez
 *
 * @brief (SOPER p3, ejercicio 7)
 * main del proceso server 
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <mqueue.h>
#include <string.h>

#include "stream.h"

/**
 * @brief accede a la estructura de datos compartidos para 
 * escribir un caracter en el buffer apuntado por post_pos. Y 
 * actualiza el valor de post_pos en caso de que no se lea 
 * '\0'. Pone el valor de *err a 1 en caso de error.
 * 
 * @param stream_shm  estructura de los datos compartidos
 * @param fd_input fichero desde el que se leen los caracteres
 * @param err vale 0, se establece su valor a 1 en caso de error
 * @return el carater leído
 */
char st_shm_add_element(struct stream_t *stream_shm, int fd_input, int *err)
{
    int ret;
    char c = 0;

    if ((ret = read(fd_input, &c, sizeof(c))) > 0)
    {
        stream_shm->buffer[stream_shm->post_pos] = c;
        (stream_shm->post_pos) = (stream_shm->post_pos + 1) % BUFFER_SIZE;
    }
    else if(ret == -1)
    {
        perror("read");
        (*err) = 1;
    }
    else
    {
        stream_shm->buffer[(stream_shm->post_pos)] = c = '\0';
    }
    return c;
}

int main(int argc, char *argv[]){
    struct stream_t *stream_shm;
    int fd_input, ret, err = 0, time_out = 0, msg;
    struct timespec ts;
    char c;
    mqd_t queue;
 

    if (argc != 2){
        fprintf(stderr, "Uso:\t%s <input_file>", argv[0]);
        exit(EXIT_FAILURE);
    }

    fd_input = open(argv[1], O_RDONLY, 0);
    if(fd_input == -1)
    {
        perror("open input file");
        exit(EXIT_FAILURE);
    }
    queue = mq_open(MQ_SERVER, O_RDONLY);
    if(queue == (mqd_t)-1)
    {
        perror("mq_open");
        close(fd_input);
        exit(EXIT_FAILURE);
    }

    /*mapeo de la memoria compartida*/
    stream_shm = st_shm_open();
    if(stream_shm == MAP_FAILED)
    {
        close(fd_input);
        mq_close(queue);
        exit(EXIT_FAILURE);
    }

    /*productor*/
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

        if(st_timed_wait(&(stream_shm->sem_empty), &ts, 2, &err, &time_out) == EXIT_FAILURE)
            break;
        else if (time_out)
            continue;
        
        if(st_timed_wait(&(stream_shm->mutex), &ts, 2, &err, &time_out) == EXIT_FAILURE)
            break;
        else if(time_out)
        {
            sem_post(&(stream_shm->sem_fill));
            continue;
        }

        c = st_shm_add_element(stream_shm, fd_input, &err);

        sem_post(&(stream_shm->mutex));
        sem_post(&(stream_shm->sem_fill));

    }while(c != '\0' && !err);

    close(fd_input);
    munmap (stream_shm, sizeof(struct stream_t));
    
    if(c == '\0' && !err)
        st_ingore_until_exit(queue, &err);

    mq_close(queue);
    
    exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
}