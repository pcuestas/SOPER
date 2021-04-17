#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include "stream.h"

char stream_shm_get_element(struct stream_t *stream_shm, int fd_output)
{
    char c = stream_shm->buffer[stream_shm->get_pos];
    int ret;
    if(c != '\0')
    {
        ret = write(fd_output, &c, sizeof(char));
        if(ret == -1){
            perror("write");
            exit(EXIT_FAILURE);
        }
        (stream_shm->get_pos) = (stream_shm->get_pos + 1) % BUFFER_SIZE;
    }
    printf("%c\n",stream_shm->buffer[stream_shm->get_pos]);
    return c;
}

int main(int argc, char *argv[]){
    struct stream_t *stream_shm;
    int fd_shm, fd_output, err = 0;
    char c;
    struct timespec ts;

    if (argc != 2){
        fprintf(stderr, "Uso:\t%s <output_file>", argv[0]);
        exit(EXIT_FAILURE);
    }

    if((fd_output = open(argv[1], O_CREAT | O_TRUNC | O_WRONLY, 0) == -1))
    {
        perror("open output file");
        exit(EXIT_FAILURE);
    }

    /*abrir el segmento de memoria compartida*/
    if ((fd_shm = shm_open(SHM_NAME, O_RDWR, 0)) == -1)
    {
        perror("shm_open");
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
        close(fd_output);
        exit(EXIT_FAILURE);
    }

    if(clock_gettime(CLOCK_REALTIME, &ts) == -1)
    {
        perror("clock_gettime");
        close(fd_output);
        munmap (stream_shm, sizeof(struct stream_t)) ;
        exit(EXIT_FAILURE);
    }

    /*consumidor*/

    do{// CONTEMPLAR RET==-1 (ERROR)
        ts.tv_sec += 2;
        if(sem_timedwait(&(stream_shm->sem_fill), &ts) == -1){
            err = 1;
            break;
        }
        ts.tv_sec += 2;
        if(sem_timedwait(&(stream_shm->mutex), &ts) == -1){
           err = 1;
            break;
        }

        c = stream_shm_get_element(stream_shm, fd_output);

        sem_post(&(stream_shm->mutex));
        sem_post(&(stream_shm->sem_empty)); 
    }while(c != '\0');

    close(fd_output);
    munmap (stream_shm, sizeof(struct stream_t)) ;

    if(err == 1)
    {
        fprintf(stderr, "Espera máxima en el semáforo agotada: %s\n", argv[0]);
        /**/
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}