#ifndef STREAM_H_
#define STREAM_H_

#include <semaphore.h>
#include <mqueue.h>

#define SHM_NAME "/shm_stream"
#define MQ_SERVER "/mq_server"
#define MQ_CLIENT "/mq_client"

#define MSG_SIZE 4
#define MSG__GET 1
#define MSG__POST 2
#define MSG__EXIT 3
#define MSG__OTHER 0

#define BUFFER_SIZE 5

struct stream_t{
    char buffer[BUFFER_SIZE];
    int post_pos;
    int get_pos;
    sem_t sem_empty;
    sem_t sem_fill;
    sem_t mutex;
};

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
int stream_timed_wait(sem_t *sem, struct timespec *ts, int seconds, int *err, int *time_out);

/**
 * @brief a partir del mensaje msg (de tamaño MSG_SIZE), 
 * devuelve uno de los siguientes enteros: 
 * MSG__GET, MSG__POST, MSG__EXIT, MSG__OTHER
 * 
 * @param msg el mensaje 
 * @return el entero que corresponde al mensaje
 */
int stream_parse_message(char *msg);

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
void ignore_messages_until_exit(mqd_t queue, int *err);


#endif