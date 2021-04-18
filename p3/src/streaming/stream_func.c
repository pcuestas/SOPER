#include "stream.h"
#include <stdio.h>
#include <mqueue.h>
#include <string.h>


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