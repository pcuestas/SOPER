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

#include "stream.h"

void read_fromFile(int fd, Stream_t *shm_struct)
{
    int nBytes = 1;
    int val, s;
    struct timespec ts;

    do
    {

        _sem_timedwait(&(shm_struct->sem_empty));
        _sem_timedwait(&(shm_struct->sem_mutex));

        nBytes = read(fd, &(shm_struct->buffer[shm_struct->post_pos]), sizeof(char));
        shm_struct->post_pos = (shm_struct->post_pos + 1) % BUFFER_SIZE;

        sem_post(&(shm_struct->sem_mutex));
        sem_post((&(shm_struct->sem_fill)));
    } while (nBytes != 0);

    _sem_timedwait(&(shm_struct->sem_empty));
    _sem_timedwait(&(shm_struct->sem_mutex));
    shm_struct->buffer[shm_struct->post_pos] = '\0';
    sem_post(&(shm_struct->sem_mutex));
    sem_post((&(shm_struct->sem_fill)));
}

/*Arguemnto 1 es el file donde leer*/
int main(int argc, char *args[])
{
    int fd, fd_shm;

    Stream_t *shm_struct = NULL;

    /*Abrir el fichero de lectura*/
    if ((fd = open(args[1], O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    /*abrir el segmento de memoria compartida*/
    if ((fd_shm = shm_open(SHM_NAME, O_RDWR, 0)) == -1)
    {
        perror("shm_open");
        close(fd);
        exit(EXIT_FAILURE);
    }

    /*mapear el segmento de memoria*/
    shm_struct = mmap(NULL, sizeof(struct stream_t),
                      PROT_WRITE | PROT_READ, MAP_SHARED, fd_shm, 0);
    close(fd_shm);
    if (shm_struct == MAP_FAILED)
    {
        perror("mmap");
        close(fd);
        exit(EXIT_FAILURE);
    }

    read_fromFile(fd, shm_struct);

    printf("He acabado \n");
    close(fd);
    getchar();
    return 0;
}

/*Arguemnto 1 es el file donde leer*/
int main(int argc, char *args[])
{
    int fd, fd_shm;

    Stream_t *shm_struct = NULL;

    /*Abrir el fichero de lectura*/
    if ((fd = open(args[1], O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    shm_struct = open_shmSegment(SHM_NAME);

    read_fromFile(fd, shm_struct);

    printf("He acabado \n");
    close(fd);
    getchar();
    return 0;
}