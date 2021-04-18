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

char stream_shm_get_element(struct stream_t *stream_shm, int fd_output)
{
    char c = stream_shm->buffer[stream_shm->get_pos];
    int ret;
    if (c != '\0')
    {
        ret = write(fd_output, &c, sizeof(c));
        if (ret == -1)
        {
            perror("write");
            exit(EXIT_FAILURE);
        }
        (stream_shm->get_pos) = (stream_shm->get_pos + 1) % BUFFER_SIZE;
    }
    return c;
}

int stream_client_parse_message(char *msg)
{
    if (strncmp(msg, "get", 3 * sizeof(char)) == 0)
        return MSG__GET;
    if (strncmp(msg, "exit", 4 * sizeof(char)) == 0)
        return MSG__EXIT;
    return MSG__OTHER;
}

void ignore_messages_until_exit(mqd_t queue, int *err)
{
    char msg[MSG_SIZE];
    int msg_meaning;
    do
    {
        if (mq_receive(queue, msg, sizeof(msg), NULL) == -1)
        {
            fprintf(stderr, "Error recibiendo el mensaje\n");
            (*err) = 1;
            break;
        }
        msg_meaning = stream_client_parse_message(msg);
    } while (msg_meaning != MSG__EXIT);
}

int main(int argc, char *argv[]){
    struct stream_t *stream_shm;
    int fd_shm, fd_output, err = 0, msg_meaning;
    char c, msg[MSG_SIZE];
    struct timespec ts;
    mqd_t queue;
    struct mq_attr mq_attributes = {
        .mq_flags = 0, 
        .mq_maxmsg = 10,
        .mq_msgsize = MSG_SIZE,
        .mq_curmsgs = 0
    };

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

    queue = mq_open(MQ_CLIENT, O_RDONLY);
    if(queue == (mqd_t)-1)
    {
        perror("mq_open");
        close(fd_output);
        exit(EXIT_FAILURE);
    }

    /*abrir el segmento de memoria compartida*/
    if ((fd_shm = shm_open(SHM_NAME, O_RDWR, 0)) == -1)
    {
        perror("shm_open");
        mq_close(queue);
        close(fd_output);
        exit(EXIT_FAILURE);
    }

    /*mapear el segmento de memoria*/
    stream_shm = mmap(NULL, sizeof(struct stream_t),
                      PROT_WRITE | PROT_READ, MAP_SHARED, fd_shm, 0);
    close(fd_shm);
    if (stream_shm == MAP_FAILED)
    {
        perror("mmap");
        mq_close(queue);
        close(fd_output);
        exit(EXIT_FAILURE);
    }


    /*consumidor*/

    do
    {
        if(mq_receive(queue, msg, sizeof(msg), NULL) == -1)
        {
            fprintf(stderr, "Error recibiendo el mensaje (%s)\n", argv[0]);
            err = 1;
            break;    
        }
        msg_meaning = stream_client_parse_message(msg);
        if(msg_meaning == MSG__EXIT)
            break;
        else if(msg_meaning != MSG__GET)
            continue;

        if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
        {
            perror("clock_gettime");
            err = 1;
            break;
        }
        ts.tv_sec += 2;
        if (sem_timedwait(&(stream_shm->sem_fill), &ts) == -1)
        {
            if (errno == ETIMEDOUT)
                fprintf(stderr, "sem_timedwait() tiempo de espera agotado\n");
            else
                perror("sem_timedwait");
            err = 1;
            break;
        }

        if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
        {
            perror("clock_gettime");
            err = 1;
            break;
        }
        ts.tv_sec += 2;
        if (sem_timedwait(&(stream_shm->mutex), &ts) == -1)
        {
            if (errno == ETIMEDOUT)
                fprintf(stderr, "sem_timedwait() tiempo de espera agotado\n");
            else
                perror("sem_timedwait");
            err = 1;
            break;
            ;
        }

        c = stream_shm_get_element(stream_shm, fd_output);

        sem_post(&(stream_shm->mutex));
        sem_post(&(stream_shm->sem_empty));
    } while (c != '\0');

    if(c == '\0')
        ignore_messages_until_exit(queue, &err);

    close(fd_output);
    mq_close(queue);
    munmap (stream_shm, sizeof(struct stream_t)) ;

    if(err == 1)
    {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}