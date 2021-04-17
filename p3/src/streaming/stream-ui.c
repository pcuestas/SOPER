#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <mqueue.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <fcntl.h>

#include "stream.h"

#define SERVER_EXEC_FILE "./stream-server"
#define CLIENT_EXEC_FILE "./stream-client"
#define INPUT_FILE argv[1]
#define OUTPUT_FILE argv[2]

/**
 * @brief inicializar los atributos de la estructura
 * de la memoria compartida
 * 
 * @param stream_shm estructura de memoria compartida
 */
void stream_shm_initialize(struct stream_t *stream_shm){
    stream_shm->post_pos = stream_shm->get_pos = 0;

    if(sem_init(&(stream_shm->mutex), 1, 1) != 0)
    {
        perror("sem_init");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    if(sem_init(&(stream_shm->sem_empty), 1, BUFFER_SIZE) != 0)
    {
        perror("sem_init");
        shm_unlink(SHM_NAME);
        sem_destroy(&(stream_shm->mutex));
        exit(EXIT_FAILURE);
    }

    if(sem_init(&(stream_shm->sem_fill), 1, 0) != 0)
    {
        perror("sem_init");
        shm_unlink(SHM_NAME);
        sem_destroy(&(stream_shm->mutex));
        sem_destroy(&(stream_shm->sem_empty));
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]){
    struct stream_t *stream_shm;
    int fd_shm;
    pid_t server, client;

    if (argc != 3){
        fprintf(stderr, "Uso:\t%s <input_file> <output_file>", argv[0]);
        exit(EXIT_FAILURE);
    }
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    shm_unlink(SHM_NAME);
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    /*creación del segmento de memoria compartida*/
    if ((fd_shm = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL,
                           S_IRUSR | S_IWUSR)) == -1)
    {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    /*truncar al tamaño de la estructura*/
    if (ftruncate(fd_shm, sizeof(struct stream_t)) == -1)
    {
        perror("ftruncate");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    /*mapear el segmento de memoria*/
    stream_shm = mmap(NULL, sizeof(struct stream_t),
                      PROT_WRITE | PROT_READ, MAP_SHARED, fd_shm, 0);
    close(fd_shm);
    if (stream_shm == MAP_FAILED)
    {
        perror("mmap");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    /*inicializar la memoria compartida*/
    stream_shm_initialize(stream_shm);

    /*lanzar los dos procesos*/
    server = fork();printf("server fork\n");
    if(server < 0)
    {
            perror("fork server");
            shm_unlink(SHM_NAME);
            exit(EXIT_FAILURE);
    }if(server == 0){
        if(execl(SERVER_EXEC_FILE, SERVER_EXEC_FILE, INPUT_FILE, (char*)NULL) == -1)
        {
            perror("execlp server");
            shm_unlink(SHM_NAME);
            exit(EXIT_FAILURE);
        }
    }

    client = fork();printf("client fork\n");
    if(client < 0)
    {
            perror("fork client");
            shm_unlink(SHM_NAME);
            exit(EXIT_FAILURE);
    }if(client == 0){
        if(execl(CLIENT_EXEC_FILE, CLIENT_EXEC_FILE, OUTPUT_FILE, (char*)NULL) == -1)
        {
            perror("execlp client");
            shm_unlink(SHM_NAME);
            exit(EXIT_FAILURE);
        }
    }


    /***************/




    /*liberar todo-- por hacer*/

    wait(NULL);// modificar  para ver errores etc
    wait(NULL);
    
    shm_unlink(SHM_NAME);
    munmap (stream_shm, sizeof(struct stream_t));

    if (sem_destroy(&(stream_shm->mutex)) == -1) {
        perror ("sem_destroy"); 
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}