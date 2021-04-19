/**
 * @file stream-ui.c
 * @author Pablo Cuesta Sierra, Álvaro Zamanillo Sáez
 *
 * @brief (SOPER p3, ejercicio 7)
 * main del proceso ui, que lanza los otros dos procesos
 * stream-server y stream-client
 */
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

/**
 * @brief crea el segmento de memoria compartida e
 * inicializa la estructura, devolviendo el puntero 
 * a la estructura mapeada
 * 
 * @return estructura de memoria compartida ya inicializada,
 * NULL en caso de error
 */
struct stream_t *st_shm_initialize(){
    int fd_shm;
    struct stream_t *stream_shm;
        
    /*creación del segmento de memoria compartida*/
    if ((fd_shm = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL,
                           S_IRUSR | S_IWUSR)) == -1)
    {
        perror("shm_open");
        return NULL;
    }

    /*truncar al tamaño de la estructura*/
    if (ftruncate(fd_shm, sizeof(struct stream_t)) == -1)
    {
        perror("ftruncate");
        close(fd_shm);
        return NULL;
    }

    /*mapear el segmento de memoria*/
    stream_shm = mmap(NULL, sizeof(struct stream_t),
                      PROT_WRITE | PROT_READ, MAP_SHARED, fd_shm, 0);
    close(fd_shm);
    if (stream_shm == MAP_FAILED)
    {
        perror("mmap");
        return NULL;
    }

    stream_shm->post_pos = stream_shm->get_pos = 0;

    if(sem_init(&(stream_shm->mutex), 1, 1) != 0)
    {
        perror("sem_init");
        shm_unlink(SHM_NAME);
        return NULL;
    }

    if(sem_init(&(stream_shm->sem_empty), 1, BUFFER_SIZE) != 0)
    {
        perror("sem_init");
        shm_unlink(SHM_NAME);
        sem_destroy(&(stream_shm->mutex));
        return NULL;
    }

    if(sem_init(&(stream_shm->sem_fill), 1, 0) != 0)
    {
        perror("sem_init");
        shm_unlink(SHM_NAME);
        sem_destroy(&(stream_shm->mutex));
        sem_destroy(&(stream_shm->sem_empty));
        return NULL;
    }
    return stream_shm;
}

/**
 * @brief lanza los dos procesos server y client
 * 
 * @param input fichero de la entrada (para server)
 * @param output fichero para la salida (para client)
 * @return EXIT_FAILURE en caso de error, EXIT_SUCCESS 
 * en el caso contrario.
 */
int st_fork_processes(char *input, char *output)
{
    pid_t server, client;
    /*lanzar los dos procesos*/
    server = fork();
    if (server < 0)
    {
        perror("fork server");
        return EXIT_FAILURE;
    }
    if (server == 0)
    {
        if (execl(SERVER_EXEC_FILE, SERVER_EXEC_FILE, input, (char *)NULL) == -1)
        {
            perror("execlp server");
            exit(EXIT_FAILURE);
        }
    }

    client = fork();
    if (client < 0)
    {
        perror("fork client");
        return EXIT_FAILURE;
    }
    if (client == 0)
    {
        if (execl(CLIENT_EXEC_FILE, CLIENT_EXEC_FILE, output, (char *)NULL) == -1)
        {
            perror("execlp client");
            exit(EXIT_FAILURE);
        }
    }
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]){
    struct stream_t *stream_shm = NULL;
    int fd_shm;
    int msg, send_to_server = 0, send_to_client = 0, err = 0;
    pid_t server, client;
    char buffer[1024];
    mqd_t queue_server, queue_client;

    if (argc != 3){
        fprintf(stderr, "Uso:\t%s <input_file> <output_file>", argv[0]);
        exit(EXIT_FAILURE);
    }

    /*creación de las colas de mensajes*/
    queue_server = st_mq_open(MQ_SERVER, O_CREAT | O_WRONLY | O_EXCL);
    if(queue_server == (mqd_t)-1)
    {
        perror("mq_open");
        exit(EXIT_FAILURE);
    }
    queue_client = st_mq_open(MQ_CLIENT, O_CREAT | O_WRONLY | O_EXCL);
    if(queue_client == (mqd_t)-1)
    {
        perror("mq_open");
        mq_close(queue_server);
        mq_unlink(MQ_SERVER);
        exit(EXIT_FAILURE);
    }
    
    /*inicializar la estructura de memoria compartida*/
    stream_shm = st_shm_initialize();
    if(stream_shm == NULL)
    {
        mq_close(queue_server);
        mq_unlink(MQ_SERVER);
        mq_close(queue_client);
        mq_unlink(MQ_CLIENT);
        exit(EXIT_FAILURE);
    }

    /*lanzar los otros dos procesos*/
    if(st_fork_processes(argv[1], argv[2]) == EXIT_FAILURE)
        err = 1;

    /*bucle de lectura de stdin y mandar mensajes a los procesos*/
    msg = MSG__OTHER;

    while (!err && (msg != MSG__EXIT) && (fgets(buffer, sizeof(buffer), stdin) != NULL))
    {
        msg = st_parse_command(buffer);
        send_to_server = (msg == MSG__POST) || (msg == MSG__EXIT);
        send_to_client = (msg == MSG__GET) || (msg == MSG__EXIT);

        if (send_to_client && (mq_send(queue_client, (char*)(&msg), sizeof(msg), 1) == -1))
        {
            perror("mq_send");
            err = 1;
            break;
        }
        if (send_to_server && (mq_send(queue_server, (char*)(&msg), sizeof(msg), 1) == -1))
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

    if(!err)
    {
        wait(NULL);
        wait(NULL);
    }
    sem_destroy(&(stream_shm->mutex));
    sem_destroy(&(stream_shm->sem_empty));
    sem_destroy(&(stream_shm->sem_fill));
    munmap(stream_shm, sizeof(struct stream_t));

    exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
}