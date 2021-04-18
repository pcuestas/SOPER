#include "stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <string.h>
#include <time.h>
#include <errno.h>


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
int stream_timed_wait(sem_t *sem, struct timespec *ts, int seconds, int *err, int *time_out)
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
 * @brief a partir del mensaje msg (de tamaño MSG_SIZE), 
 * devuelve uno de los siguientes enteros: 
 * MSG__GET, MSG__POST, MSG__EXIT, MSG__OTHER
 * 
 * @param msg el mensaje 
 * @return el entero que corresponde al mensaje
 */
int stream_parse_message(char *msg)
{
    if (strncmp(msg, "post", 4 * sizeof(char)) == 0)
        return MSG__POST;
    if (strncmp(msg, "get", 3 * sizeof(char)) == 0)
        return MSG__GET;
    if (strncmp(msg, "exit", 4 * sizeof(char)) == 0)
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
void ignore_messages_until_exit(mqd_t queue, int *err)
{
    char msg[MSG_SIZE];
    int msg_meaning;
    do
    {
        if (mq_receive(queue, msg, sizeof(msg), NULL) == -1)
        {
            fprintf(stderr, "Error recibiendo el mensaje\n");
            (*err) = 1;
            break;
        }
        msg_meaning = stream_parse_message(msg);
    } while (msg_meaning != MSG__EXIT);
}
