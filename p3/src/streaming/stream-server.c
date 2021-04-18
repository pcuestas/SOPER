#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

#include "stream.h"

int stream_shm_add_element(struct stream_t *stream_shm, int fd_input)
{
    int ret;
    
    if((ret = read(fd_input, &(stream_shm->buffer[stream_shm->post_pos]), sizeof(char))) > 0)
    {
        (stream_shm->post_pos) = (stream_shm->post_pos + 1) % BUFFER_SIZE;
    }else
    {
        stream_shm->buffer[(stream_shm->post_pos)] = '\0';
    }
    return ret;
}

int main(int argc, char *argv[]){
    struct stream_t *stream_shm;
    int fd_shm, fd_input, ret, err = 0;
    struct timespec ts;
    char aux;

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

    /*abrir del segmento de memoria compartida*/
    if ((fd_shm = shm_open(SHM_NAME, O_RDWR, 0)) == -1)
    {
        perror("shm_open");
        close(fd_input);
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
        exit(EXIT_FAILURE);
    }

    if(clock_gettime(CLOCK_REALTIME, &ts) == -1)
    {
        perror("clock_gettime");
        close(fd_input);
        munmap (stream_shm, sizeof(struct stream_t)) ;
        exit(EXIT_FAILURE);
    }

    /*productor*/

    do{// CONTEMPLAR RET==-1 (ERROR)
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
        if(sem_timedwait(&(stream_shm->sem_empty), &ts) == -1){
            err = 1;
            break;
        }
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 2;
        if(sem_timedwait(&(stream_shm->mutex), &ts) == -1){
            err = 1;
            break;
        }
        
        ret = stream_shm_add_element(stream_shm, fd_input);

        sem_post(&(stream_shm->mutex));
        sem_post(&(stream_shm->sem_fill));

    }while(ret > 0);

    close(fd_input);
    munmap (stream_shm, sizeof(struct stream_t)) ;
    
    if(err == 1)
    {
        fprintf(stderr, "Espera máxima en el semáforo agotada: %s\n", argv[0]);
        /**/
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}