#include <stdio.h>
#include <stdlib.h>
//#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "stream.h"

#define READ_FILE "read_file.txt"
#define WRITE_FILE "write_file.txt"



/**
 * Función que lanza dos procesos hijos. Uno que ejecuta stream-server y otro, stream-client
 */
void init_stream_proccess(char *readfile, char *writefile){
    pid_t pid;

    pid = fork();

    if (pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        if (execlp("./reader", "./reader", readfile, SHM_NAME, (char *)NULL))
        {
            perror("execvp 1");
            exit(EXIT_FAILURE);
        }
    }
    printf("Priemr proceso: %i \n", pid);
    /*pid = fork();

    if (pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        if (execlp("./stream-client", "./stream-client", writefile, SHM_NAME, (char *)NULL))
        {
            perror("execvp 2");
            exit(EXIT_FAILURE);
        }
    }
    printf("Segundo proceso: %i \n", pid);*/
}

/**
 * Crea un segmento de memoria compartido.
 */
Stream_t *init_shm_segment()
{
    shm_unlink(SHM_NAME);
    int fd_shm;
    Stream_t *shm_struct;

    if ((fd_shm = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)) == -1){
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd_shm, sizeof(Stream_t)) == -1){
        perror("ftruncate");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    shm_struct = mmap(NULL, sizeof(Stream_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);

    close(fd_shm);
    if (shm_struct == MAP_FAILED)
    {
        perror("mmap");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    shm_struct->get_pos = 0;
    shm_struct->post_pos = 0;

    /*Init semphores*/

    if (sem_init(&(shm_struct->sem_empty), 1, BUFFER_SIZE) == -1)
    {
        perror("sem init: ");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    if (sem_init(&(shm_struct->sem_mutex), 1, 1) == -1)
    {
        perror("sem init: ");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    if (sem_init(&(shm_struct->sem_fill), 1, 0) == -1)
    {
        perror("sem init: ");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    return shm_struct;
}

int main(int argc, char *args[])
{

    int i, fd_shm;
    Stream_t *shm_struct = NULL;
    pid_t pid;

    if (argc != 3)
    {
        fprintf(stderr, "Two arguments needed.\n./stream-ui <READ_FILE> <WRITE_FILE>");
        return 0;
    }

    /*Creamos el segmento de memoria compartido*/
    shm_struct = init_shm_segment();

    /*Lanzar los dos procesos stream-server y stream-client*/
    init_stream_proccess(args[1], args[2]);
    printf("Los he lanzado");

    /*Esperar a los dos procesos hijo*/
    wait(NULL);
    getchar();



    printf("LEIDO %s",shm_struct->buffer);

    pid = fork();

    if (pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        if (execlp("./writer", "./writer", args[2], SHM_NAME, (char *)NULL)){
            perror("execvp 2");
            exit(EXIT_FAILURE);
        }
    }
    printf("Segundo proceso: %i \n", pid);


        /*Liberar memoria y semaforos*/
    sem_destroy(&(shm_struct->sem_empty));
    sem_destroy(&(shm_struct->sem_fill));
    sem_destroy(&(shm_struct->sem_mutex));

    munmap(shm_struct, sizeof(Stream_t));

    printf("Yaaaa\n");

    return 0;
}