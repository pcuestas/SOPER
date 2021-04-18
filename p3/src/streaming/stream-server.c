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
    int fd_shm, fd_input, ret, err = 0, msg_meaning, time_out = 0;
    struct timespec ts;
    char c, msg[MSG_SIZE];
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
        msg_meaning = st_parse_message(msg);
        if(msg_meaning == MSG__EXIT)
            break;
        else if(msg_meaning != MSG__POST)
            continue;

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

    if(c == '\0' && !err)
        st_ingore_until_exit(queue, &err);

    close(fd_input);
    mq_close(queue);
    munmap (stream_shm, sizeof(struct stream_t)) ;
    
    if(err)
    {
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}