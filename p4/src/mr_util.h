/**
 * @file mr_util.h (Proyecto SOPER)
 * @author Pablo Cuesta Sierra, Álvaro Zamanillo Sáez
 * @brief Funciones comunes a mr_miner.c y mr_monitor.c
 */
#ifndef MR_UTIL_H
#define MR_UTIL_H

#include "miner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mqueue.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define SEM_MUTEX_NAME "/mr_mutex"
#define MQ_MONITOR_NAME "/mr_mq_monitor"

#define MR_SHM_FAILED -1
#define MR_SHM_CREATED 1
#define MR_SHM_EXISTS 0

#define MSG_SIZE (sizeof(Block))

/**
 * @brief mapea un segmento de memoria compartida, creándolo 
 * en caso de que no exista, con un tamaño determinado.
 * 
 * @param file_name nombre del segmento de memoria compartida
 * @param p en *p se guarda el puntero a la memoria mapeada
 * @param size tamaño del segmento 
 * 
 * @return MR_SHM_CREATED en caso de que la memoria se haya creado 
 * con éxito. 
 * @return MR_SHM_FAILED en caso de error
 * @return MR_SHM_EXISTS en caso de que la memoria exista y se abra 
 * con éxito
 */
int mr_shm_map(char* file_name, void **p, size_t size);

/**
 * @brief abre la cola con nombre queue_name con 
 * las banderas __oflag. El tamaño de mensajes
 * se pone al valor de la constante MSG_SIZE.
 * 
 * @param queue_name nombre de la cola a abrir
 * @param __oflag igual que __oflag en mq_open
 * @return la cola abierta, o (mdq_t)-1 en caso de error
 */
mqd_t mr_mq_open(char *queue_name, int __oflag);

/**
 * @brief Libera la cadena de bloques entera
 * 
 * @param last_block último bloque de la cadena
 */
void mr_blocks_free(Block *last_block);

/**
 * @brief Reserva memoria para un nuevo bloque con la 
 * información de shm_b y lo pone en la cadena a la que 
 * apunta last_block como nuevo último bloque.
 * 
 * @param shm_b bloque que se copia
 * @param last_block último bloque
 * @return puntero al nuevo bloque creado (nuevo último bloque)
 */
Block* mr_block_append(Block *shm_b, Block *last_block);

/**
 * @brief realiza un sem_timedwait de seconds segundos 
 * en el semáforo sem. Ignora las posibles interrupciones
 * por señales y devuelve solo cuando hay error, se agota el 
 * tiempo de espera, o se baja el semáforo con éxito. 
 * En caso de algún error (o que se agote el tiempo), imprime por stderr
 * el mensaje de error correspondiente y devuelve 1;
 * 
 * @param sem semáforo en el que se realiza la espera
 * @param seconds segundos 
 * @param time_out toma el valor 1 si hay fallo por espera agotada.
 * 0 en caso contrario
 * 
 * @return 1 en caso de que falle clock_gettime
 * o sem_timedwait porque se agota el tiempo.
 * 0 en caso de éxito
 */
int mr_sem_timedwait(sem_t *sem, int seconds, int* time_out);


/**
 * @brief intenta mandar un mensaje a una cola. Si la cola está llena se quedará
 * bloqueado un tiempo máximo establecido.
 * En caso de algún error (o que se agote el tiempo), imprime por stderr
 * el mensaje de error correspondiente y devuelve 1;
 * 
 * @param queue cola de mensajes a la que se envía
 * @param last_block cadena de bloques con el bloque que se va a enviar
 * @param priority prioridad del mensaje
 * @param seconds segundos de espera máima
 * @param time_out toma el valor 1 si hay fallo por espera agotada.
 * 0 en caso contrario
 * 
 * @return 1 en caso de que falle clock_gettime
 * o mq_timedsend porque se agota el tiempo.
 * 0 en caso de éxito
 */
int mr_mq_timedsend(mqd_t queue, Block **last_block, int priority, int seconds, int *time_out);

/**
 * @brief Imprime la cadena de bloques entera al inicio de 
 * un fichero
 * 
 * @param plast_block úlitimo bloque de la cadena
 * @param num_wallets número de wallets a imprimir
 * @param fd descriptor del fichero abierto en el que se imprime
 */
void mr_blocks_print_to_file(Block *plast_block, int num_wallets, int fd);

#endif
