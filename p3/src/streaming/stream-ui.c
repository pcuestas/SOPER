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
 * @return EXIT_FAILURE en caso de error (con todos los 
 * semáforos destruidos), EXIT_SUCCESS en caso contrario
 */
int stream_shm_initialize(struct stream_t *stream_shm){
    stream_shm->post_pos = stream_shm->get_pos = 0;

    if(sem_init(&(stream_shm->mutex), 1, 1) != 0)
    {
        perror("sem_init");
        shm_unlink(SHM_NAME);
        return EXIT_FAILURE;
    }

    if(sem_init(&(stream_shm->sem_empty), 1, BUFFER_SIZE) != 0)
    {
        perror("sem_init");
        shm_unlink(SHM_NAME);
        sem_destroy(&(stream_shm->mutex));
        return EXIT_FAILURE;
    }

    if(sem_init(&(stream_shm->sem_fill), 1, 0) != 0)
    {
        perror("sem_init");
        shm_unlink(SHM_NAME);
        sem_destroy(&(stream_shm->mutex));
        sem_destroy(&(stream_shm->sem_empty));
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]){
    struct stream_t *stream_shm;
    int fd_shm;
    int msg_meaning, send_to_server = 0, send_to_client = 0, err = 0;
    pid_t server, client;
    char msg[MSG_SIZE], buffer[1024];
    mqd_t queue_server, queue_client;
    struct mq_attr mq_attributes = {
        .mq_flags = 0, 
        .mq_maxmsg = 10,
        .mq_msgsize = MSG_SIZE,
        .mq_curmsgs = 0
    };

    if (argc != 3){
        fprintf(stderr, "Uso:\t%s <input_file> <output_file>", argv[0]);
        exit(EXIT_FAILURE);
    }

    /*creación de las colas de mensajes*/
    queue_server = mq_open(MQ_SERVER, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR, &mq_attributes);
    if(queue_server == (mqd_t)-1)
    {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }
    queue_client = mq_open(MQ_CLIENT, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR, &mq_attributes);
    if(queue_client == (mqd_t)-1)
    {
        perror("mq_open");
        mq_close(queue_server);
        mq_unlink(MQ_SERVER);
        exit(EXIT_FAILURE);
    }
    
    /*creación del segmento de memoria compartida*/
    if ((fd_shm = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL,
                           S_IRUSR | S_IWUSR)) == -1)
    {
        perror("shm_open");
        mq_close(queue_server);
        mq_close(queue_client);
        mq_unlink(MQ_CLIENT);
        mq_unlink(MQ_SERVER);
        exit(EXIT_FAILURE);
    }

    /*truncar al tamaño de la estructura*/
    if (ftruncate(fd_shm, sizeof(struct stream_t)) == -1)
    {
        perror("ftruncate");
        mq_close(queue_server);
        mq_close(queue_client);
        mq_unlink(MQ_CLIENT);
        mq_unlink(MQ_SERVER);
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
        mq_close(queue_server);
        mq_close(queue_client);
        mq_unlink(MQ_CLIENT);
        mq_unlink(MQ_SERVER);
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }
    /*inicializar la memoria compartida*/
    if (stream_shm_initialize(stream_shm) == EXIT_FAILURE)
    {
        mq_close(queue_server);
        mq_close(queue_client);
        mq_unlink(MQ_CLIENT);
        mq_unlink(MQ_SERVER);
        shm_unlink(SHM_NAME);
        munmap(stream_shm, sizeof(struct stream_t));
        exit(EXIT_FAILURE);
    }

    /*lanzar los dos procesos*/
    server = fork();
    if (server < 0)
    {
        perror("fork server");
        mq_close(queue_server);
        mq_close(queue_client);
        mq_unlink(MQ_CLIENT);
        mq_unlink(MQ_SERVER);
        shm_unlink(SHM_NAME);
        sem_destroy(&(stream_shm->mutex));
        sem_destroy(&(stream_shm->sem_empty));
        sem_destroy(&(stream_shm->sem_fill));
        munmap(stream_shm, sizeof(struct stream_t));
        exit(EXIT_FAILURE);
    }
    if (server == 0)
    {
        if (execl(SERVER_EXEC_FILE, SERVER_EXEC_FILE, INPUT_FILE, (char *)NULL) == -1)
        {
            perror("execlp server");
            exit(EXIT_FAILURE);
        }
    }

    client = fork();
    if (client < 0)
    {
        perror("fork client");
        mq_close(queue_server);
        mq_close(queue_client);
        mq_unlink(MQ_CLIENT);
        mq_unlink(MQ_SERVER);
        shm_unlink(SHM_NAME);
        sem_destroy(&(stream_shm->mutex));
        sem_destroy(&(stream_shm->sem_empty));
        sem_destroy(&(stream_shm->sem_fill));
        munmap(stream_shm, sizeof(struct stream_t));
        exit(EXIT_FAILURE);
    }
    if (client == 0)
    {
        if (execl(CLIENT_EXEC_FILE, CLIENT_EXEC_FILE, OUTPUT_FILE, (char *)NULL) == -1)
        {
            perror("execlp client");
            exit(EXIT_FAILURE);
        }
    }

    /*bucle de lectura de stdin y mandar mensajes a los procesos*/
    msg_meaning = MSG__OTHER;
    while ((msg_meaning != MSG__EXIT) && (fgets(buffer, sizeof(buffer), stdin) != NULL))
    {
        msg_meaning = stream_parse_message(buffer);
        send_to_server = (msg_meaning == MSG__POST) || (msg_meaning == MSG__EXIT);
        send_to_client = (msg_meaning == MSG__GET) || (msg_meaning == MSG__EXIT);

        if (send_to_client && (mq_send(queue_client, buffer, MSG_SIZE, 1) == -1))
        {
            perror("mq_send");
            err = 1;
            break;
        }
        if (send_to_server && (mq_send(queue_server, buffer, MSG_SIZE, 1) == -1))
        {
            perror("mq_send");
            err = 1;
            break;
        }
    }
    mq_close(queue_server);
    mq_close(queue_client);
    mq_unlink(MQ_CLIENT);
    mq_unlink(MQ_SERVER);
    shm_unlink(SHM_NAME);
    sem_destroy(&(stream_shm->mutex));
    sem_destroy(&(stream_shm->sem_empty));
    sem_destroy(&(stream_shm->sem_fill));
    munmap(stream_shm, sizeof(struct stream_t));

    if(err)
    {
        exit(EXIT_FAILURE);
    }

    wait(NULL);// modificar  para ver errores etc ? 
    wait(NULL);
    
    

    exit(EXIT_SUCCESS);
}