/**
 * @file mr_monitor.h (Proyecto SOPER)
 * @author Pablo Cuesta Sierra, Álvaro Zamanillo Sáez
 * @brief Funciones del monitor.
 */
#ifndef MR_MONITOR_H_
#define MR_MONITOR_H_

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

#define MAX_BLOCKS 10
#define PRINTER_WAIT 5

/**
 * @brief Estructura del monitor
 * 
 */
typedef struct _MonitorBlocks{
    Block buffer[MAX_BLOCKS]; /*buffer de bloques*/
    int last_block; /*índice del último bloque*/
}MonitorBlocks;

/**
 * @brief manejador de SIGINT. 
 * Pone got_sigint a 1
 * 
 * @param sig señal recibida
 */
void handler_sigint(int sig);

/**
 * @brief manejador de SIGALRM. 
 * Pone got_sigalrm a 1
 * 
 * @param sig señal recibida
 */
void handler_sigalrm(int sig);


/**
 * @brief hijo
 * 
 * @param fd 
 */
void mr_monitor_printer(int fd[2]);

/**
 * @brief mapea e inicializa la memoria compartida 
 * NetData (con nombre SHM_NAME_NET)
 * 
 * @param d *d contiene al terminar la función el puntero 
 * a la memoria compartida mapeada
 * @return EXIT_FAILURE en caso de error. EXIT_SUCCESS en caso de éxito.
 */
int mrt_shm_init(NetData **d);

/**
 * @brief recibir un mensaje de la cola
 * 
 * @param block apunta a donde se guarda el mensaje que se va a leer
 * @param queue cola de la que se lee el mensaje
 * 
 * @return 1 si hay error (o si se recibe señal mientras se espera a mensaje)
 * @return 0 en caso de éxito. 
 */
int mrt_mq_receive(Block *block, mqd_t queue);

/**
 * @brief Comprueba si el bloque b está ya en blocks. 
 * Si está, comprueba que la target y la solución coincida, y 
 * devuelve 1. 
 * Si no está, lo añade y devuelve 0. En caso de error, 
 * *err se pone a 1 y se devuelve 1;
 * 
 * @param b bloque a comprobar
 * @param blocks estructura de bloques del monitor donde se comprueba
 * @param err puntero cuyo valor se establece a 1 en caso de error
 * 
 * @return 0 si b no está en blocks y se añade con éxito.
 * @return 1 en caso contrario (o error - En caso de error, *err se pone a 1.
 */
int mrt_block_is_repeated(Block *b, MonitorBlocks *blocks, int *err);

/**
 * @brief lee un bloque entero de la tubería fd ignorando señales 
 * excepto SIGALRM, en caso de recibirla, se maneja (llamando a 
 * mrp_handle_sigalrm) y se continúa la lectura
 * 
 * @param block bloque a leer
 * @param fd tubería
 * @param last_block cadena de bloques del proceso 
 * @param n_wallets número de wallets
 * @param file fichero al que se imprime cada vez que se recibe SIGALRM
 * @param err *err toma el valor 1 en caso de error
 * @return número de bytes leídos. 
 * @return 0 en caso de que se lea EOF o de que haya error 
 */
int mrp_fd_read_block(Block *block, int fd[2], Block* last_block, int n_wallets, int file, int *err);

/**
 * @brief escribe un bloque entero de la tubería fd ignorando posibles
 * interrupciones por señales
 * 
 * @param block bloque que se escribe 
 * @param fd tubería
 * 
 * @return 1 en caso de error
 * @return 0 en caso de éxito
 */
int mrt_fd_write_block(Block *block, int fd[2]);

/**
 * @brief crea una nueva alarma para dentro de PRINTER_WAIT 
 * segundos e imprime la cadena de bloques en el fichero
 * solamente si n_wallets es distinto de 0 (> 0)
 * 
 * @param last_block cadena de bloques
 * @param n_wallets número de wallets
 * @param file descriptor del fichero en el que se escribe
 * 
 * @return 1 en caso de error
 * @return 0 en caso de éxito
 */
int mrp_handle_sigalrm(Block *last_block, int n_wallets, int file);

/**
 * @brief libera el mutex y la memoria compartida del monitor.
 * En caso de ser el último proceso enganchado a la memoria, 
 * hace unlink del mutex, los segmentos de memoria compartida del 
 * proceso y la cola de mensajes
 * 
 * @param mutex mutex
 * @param s_net_data segmento compartido 
 */
void mrt_close_net_mutex(sem_t *mutex, NetData* s_net_data);

#endif