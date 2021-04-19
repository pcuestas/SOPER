/**
 * @file stream_func.c
 * @author Pablo Cuesta Sierra, Álvaro Zamanillo Sáez
 *
 * @brief implementación de las funciones de stream.h
 */
#include "stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>


/**
 * @brief realiza un sem_timedwait de seconds segundos 
 * en el semáforo sem. En caso de algún error, imprime por stderr
 * el mensaje de error correspondiente y devuelve EXIT_FAILURE.
 * Si pasa el tiempo de espera sin poder aumentar el semáforo,
 * se devuelve EXIT_SUCCESS y se cambia el valor de *time_out a 1.
 * (en caso contrario, time_out termina con valor 0)
 * *err cambia de valor a 1 en caso de error.
 * 
 * @param sem semáforo en el que se realiza la espera
 * @param ts struct timespec
 * @param seconds segundos 
 * @param err cambia de valor a 1 en caso de que se devuelva 
 * EXIT_FAILURE
 * @param time_out vale 1 en caso de que se pase 
 * el tiempo de espera. 0 en caso contrario 
 * @return EXIT_FAILURE en caso de que falle clock_gettime
 * o sem_timedwait. EXIT_SUCCCESS en caso de éxito
 */
int st_timed_wait(sem_t *sem, struct timespec *ts, int seconds, int *err, int *time_out)
{
    (*time_out) = 0;
    if (clock_gettime(CLOCK_REALTIME, ts) == -1)
    {
        perror("clock_gettime");
        (*err) = 1;
        return EXIT_FAILURE;
    }
    ts->tv_sec += seconds;
    if (sem_timedwait(sem, ts) == -1)
    {
        if (errno == ETIMEDOUT)
        {
            fprintf(stderr, "sem_timedwait() tiempo de espera agotado. Operación desechada.\n");
            (*time_out) = 1;
        }
        else
        {
            perror("sem_timedwait");
            (*err) = 1;
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

/**
 * @brief a partir del mensaje msg (de longitud 3 o 4), 
 * devuelve uno de los siguientes enteros: 
 * MSG__GET, MSG__POST, MSG__EXIT, MSG__OTHER
 * 
 * @param msg el mensaje 
 * @return el entero que corresponde al mensaje
 */
int st_parse_command(char *msg)
{
    if ((strncmp(msg, "post", 4 * sizeof(char)) == 0) && (msg[4] == '\0' || msg[4] == '\n'))
        return MSG__POST;
    if (strncmp(msg, "get", 3 * sizeof(char)) == 0 && (msg[3] == '\0' || msg[3] == '\n'))
        return MSG__GET;
    if (strncmp(msg, "exit", 4 * sizeof(char)) == 0 && (msg[4] == '\0' || msg[4] == '\n'))
        return MSG__EXIT;
    return MSG__OTHER;
}

/**
 * @brief ignora los mensajes recibidos en la cola de 
 * mensajes queue hasta recibir "exit".
 * Cuando se recibe "exit", devuelve.
 * 
 * @param queue la cola de mensajes (se recibe y se 
 * deja abierta)
 * @param err en la entrada, *err=0 y se le da el valor 1
 * en caso de error (al recibir algún mensaje)
 */
void st_ingore_until_exit(mqd_t queue, int *err)
{
    int msg;
    do
    {
        if (mq_receive(queue, (char*)(&msg), sizeof(msg), NULL) == -1)
        {
            fprintf(stderr, "Error recibiendo el mensaje\n");
            (*err) = 1;
            break;
        }
    } while (msg != MSG__EXIT);
}

/**
 * @brief abre la cola con nombre queue_name con 
 * las banderas __oflag
 * 
 * @param queue_name nombre de la cola a abrir
 * @param __oflag igual que __oflag en mq_open
 * @return la cola abierta, o (mdq_t)-1 en caso de error
 */
mqd_t st_mq_open(char *queue_name, int __oflag)
{
    struct mq_attr attributes = {
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_curmsgs = 0,
        .mq_msgsize = MSG_SIZE
    };

    mqd_t queue = mq_open(queue_name, __oflag,
                          S_IRUSR | S_IWUSR,
                          &attributes);
    if (queue == (mqd_t)-1)
        perror("mq_open");

    return queue;
}

/**
 * @brief abre el segmento de memoria y lo mapea.
 * (Para el stream-server y stream-client)
 * 
 * @return puntero a la estructura mapeada o MAP_FAILED 
 * en caso de error
 */
struct stream_t *st_shm_open()
{
    int fd_shm;
    struct stream_t *stream_shm = NULL;
    /*abrir del segmento de memoria compartida*/
    if ((fd_shm = shm_open(SHM_NAME, O_RDWR, 0)) == -1)
    {
        perror("shm_open");
        return MAP_FAILED;
    }

    /*mapear el segmento de memoria*/
    stream_shm = mmap(NULL, sizeof(struct stream_t),
                      PROT_WRITE | PROT_READ, MAP_SHARED, fd_shm, 0);
    close(fd_shm);
    if (stream_shm == MAP_FAILED)
        perror("mmap");

    return stream_shm;
}