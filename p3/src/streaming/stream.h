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