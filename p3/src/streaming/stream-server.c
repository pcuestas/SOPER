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


int stream_shm_add_element(struct stream_t *stream_shm, int fd_input)
{
    int ret;

    if ((ret = read(fd_input, &(stream_shm->buffer[stream_shm->post_pos]), sizeof(char))) > 0)
    {
        (stream_shm->post_pos) = (stream_shm->post_pos + 1) % BUFFER_SIZE;
    }
    else
    {
        stream_shm->buffer[(stream_shm->post_pos)] = '\0';
    }
    return ret;
}

int main(int argc, char *argv[]){
    struct stream_t *stream_shm;
    int fd_shm, fd_input, ret, err = 0, msg_meaning;
    struct timespec ts;
    char msg[MSG_SIZE];
    mqd_t queue;
    struct mq_attr mq_attributes = {
        .mq_flags = 0, 
        .mq_maxmsg = 10,
        .mq_msgsize = MSG_SIZE,
        .mq_curmsgs = 0
    };

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

    /*abrir del segmento de memoria compartida*/
    if ((fd_shm = shm_open(SHM_NAME, O_RDWR, 0)) == -1)
    {
        perror("shm_open");
        close(fd_input);
        mq_close(queue);
        exit(EXIT_FAILURE);
    }

    /*mapear el segmento de memoria*/
    stream_shm = mmap(NULL, sizeof(struct stream_t),
                      PROT_WRITE | PROT_READ, MAP_SHARED, fd_shm, 0);
    close(fd_shm);
    if (stream_shm == MAP_FAILED)
    {
        perror("mmap");
        close(fd_input);
        mq_close(queue);
        exit(EXIT_FAILURE);
    }

    /*productor*/
    do
    {
        if(mq_receive(queue, msg, sizeof(msg), NULL) == -1)
        {
            fprintf(stderr, "Error recibiendo el mensaje (%s)\n", argv[0]);
            err = 1;
            break;    
        }
        msg_meaning = stream_parse_message(msg);
        if(msg_meaning == MSG__EXIT)
            break;
        else if(msg_meaning != MSG__POST)
            continue;
        
        if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
        {
            perror("clock_gettime");
            err = 1;
            break;
        }
        ts.tv_sec += 2;
        if(sem_timedwait(&(stream_shm->sem_empty), &ts) == -1)
        {
            if(errno == ETIMEDOUT)
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
        }


        ret = stream_shm_add_element(stream_shm, fd_input);

        sem_post(&(stream_shm->mutex));
        sem_post(&(stream_shm->sem_fill));

    }while(ret > 0);

    if(ret <= 0)
        ignore_messages_until_exit(queue, &err);
        

    close(fd_input);
    mq_close(queue);
    munmap (stream_shm, sizeof(struct stream_t)) ;
    
    if(err)
    {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}