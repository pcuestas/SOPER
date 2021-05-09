/**
 * @file mr_miner.h (Proyecto SOPER)
 * @author Pablo Cuesta Sierra, Álvaro Zamanillo Sáez
 * @brief Funciones del minero.
 */
#ifndef MR_MINER_H
#define MR_MINER_H

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

#define VOTE_YES 1
#define VOTE_NO 0
#define VOTE_NOT_VOTED 2


/**
 * @brief manejador de señales del minero
 * 
 * @param sig 
 */
void handler_miner(int sig);

/**
 * @brief establece los manejadores de señales del minero
 * con la máscara mask para los handlers. Se ignoran 
 * SIGUSR1 y SIGUSR2. Se pone el manejador handler_miner
 * para SIGINT y SIGHUP.
 * 
 * @param mask máscara a utilizar en los handlers
 * @return EXIT_FAILURE en caso de error, 
 * EXIT_SUCCESS en caso de éxito
 */
int mrr_set_handlers(sigset_t mask);

/**
 * @brief inicializar la memoria compartida, creando los segmentos
 * en caso de que no existan y mapeándolos. 
 * Si este proceso es el creador del bloque (block), se inicializan 
 * los campos de la estructura.
 * Si este proceso es el creador o si el creador ha sido el monitor y 
 * este es el primer minero en llegar, se inicializan los valores del 
 * netdata. 
 * En el caso de que este sea el primer minero activo, se 
 * inicializan los semáforos también y se declara ganador para preparar 
 * la  siguiente ronda.
 * 
 * @param b *b apuntará al bloque mapeado 
 * @param d *d apuntará a la netdata mapeada
 * @param this_index *this_index tendrá el valor del índice de este 
 * minero en el array de miners_pid y demás.
 * @return EXIT_FAILURE en caso de error, 
 * EXIT_SUCCESS en caso de éxito
 */
int mrr_shm_init(Block **b, NetData **d, int *this_index);

/**
 * @brief prepara los valores de los segmentos compartidos 
 * al inicio de una nueva ronda
 * 
 * @param b bloque compartido
 * @param d netdata compartida
 */
void mrr_set_round(Block *b, NetData *d);

/**
 * @brief manda SIGUSR1 a todos los mineros que tienen su pid en 
 * el array de net->miners_pid y actualiza el valor net->total_miners
 * con el número de mineros cuyo envío es exitoso (kill devuelve 0)
 * 
 * @param net netdata (memoria compartida)
 */
void mrr_quorum(NetData *net);

/**
 * @brief pone la solución en el bloque (compartido)
 * 
 * @param b bloque compartido
 * @param solution solución subida por este minero
 */
void mrr_set_solution(Block *b, long int solution);

/**
 * @brief configura las máscaras del minero. Se bloquean en el proceso
 * las señales: SIGUSR1, SIGUSR2, SUGHUP y SIGINT.
 * 
 * @param mask se guardan las señales bloqueadas por el proceso
 * @param mask_wait_workers contiene las señales bloqueadas por el proceso, 
 * excepto SIGUSR2 y SIGHUP (para que se puedan recibir cuando el minero
 * espere a que se encuantre una solución)
 * @param old_mask guarda la antigua máscara del proceso (antes de llamar 
 * a la función)
 */
void mrr_masks_set_up(sigset_t *mask, sigset_t *mask_wait_workers, sigset_t *old_mask);

/**
 * @brief llamada por el minero ganador. 
 * Comprueba los votos
 * 
 * @param net netdata (shm)
 * @return 1 en caso de que la votación sea favorable
 * (al menos la mitad de votantes votan a favor). 
 * 0 en caso contrario.
 */
int mrr_check_votes(NetData *net);

/**
 * @brief votar la solución subida a memoria compartida. El último 
 * votante notifica al ganador la finalización de la votación. Para esto,
 * se utiliza el mecanismo "lightswitchoff"/"lightswitch unlock".
 * 
 * @param mutex mutex que protege 
 * @param net netdata (shm) para acceder a la voting_pool
 * @param b bloque (shm) para ver si el target y la solución se 
 * corresponden correctamente
 * @param this_index índice de este minero en las estructuras compartidas
 */
void mrr_vote(sem_t *mutex, NetData *net, Block *b, int index);

/**
 * @brief ganador avisa a los votantes que ha terminado el escrutinio
 * 
 * @param net netdata (shm)
 * @param n_voters número de votantes
 */
void mrr_send_end_scrutiny(NetData *net, int n);

/**
 * @brief Mecanismo lightswitchoff, o lightswitch unlock.
 * El último proceso en disminuir el contador (el que llega a 0)
 * hace up del semáforo. Se protege el acceso al contador con 
 * un mutex.
 * 
 * @param mutex mutex
 * @param count puntero al contador
 * @param sem semáforo que se hace up en caso de ser el último
 */
void mr_lightswitchoff(sem_t *mutex, int *count, sem_t *sem);

/**
 * @brief imprime la cadena de bloques en un fichero llamado: [pid].log
 * 
 * @param last_block cadena de bloques
 * @param n_wallets número de wallets
 */
void mrr_print_chain(Block *last_block, int n_wallets);

/**
 * @brief este minero (ganador) notifica a los perdedores
 * que ha encontrado una solución mandando la señal SIGUSR2
 * 
 * @param net netdata (shm)
 */
void mrr_notify_miners(NetData *net);

/**
 * @brief el último ganador prepara una nueva ronda (calcula el quorum)
 * y avisa a todos los mineros para que comiencen la ronda con el semáforo
 * s_net_data->sem_round_begin 
 * 
 * @param s_block bloque (shm)
 * @param s_net_data netdata (shm)
 */
void mrr_last_winner_prepare_round(Block* s_block, NetData* s_net_data);

/**
 * @brief acciones del ganador de una ronda:
 * Notifica a los perdedores que se ha encontrado solución.
 * Les indica que pueden empezar a votar.
 * Si hay votación, actualiza el bloque tanto en caso de que se vote 
 * que sí, como que no. Si no la hay, lo actualiza de todas formas.
 * Impide el paso de nuevos mineros a la ronda (para evitar que los
 * ninguno empiece antes de que todos hayan acabado la ronda)
 * Por último, avisa al resto de que se ha acabado el escrutinio 
 * (sem_scrutiny).
 * 
 * @param s_block block (shm)
 * @param s_net_data netdata (shm)
 * @param this_index índice del minero en la memoria compartida
 * @return 1 en caso de error (tiempo de espera agotado),
 * 0 en caso contrario
 */
int mrr_winner_actions(Block* s_block, NetData* s_net_data, int this_index);

/**
 * @brief actualiza la memoria compartida tras una votación favorable
 * 
 * @param s_block block (shm)
 * @param s_net_data netdata (shm)
 * @param this_index índice del minero en la memoria compartida
 */
void mrr_winner_update_after_votation(Block* s_block, NetData* s_net_data, int this_index);

/**
 * @brief cierra el mutex y el mapeo de la memoria compartida.
 * Si el minero es el último minero activo, destruye los semáforos 
 * de la memoria compartida. Si además no hay monitor activo, hace 
 * unlink de la cola de mensajes, de el mutex y de los segmentos
 * de la memoria compartida
 * 
 * @param mutex mutex
 * @param s_net_data netdata (shm)
 */
void mrr_close_net_mutex(sem_t *mutex, NetData* s_net_data);

/**
 * @brief un minero que se declara ganador pero es votado 
 * que el bloque no ees válido, actualiza el bloque para que
 * la siguiente ronda se configure correctamente
 * 
 * @param s_block bloque (shm)
 */
void mrr_loser_modify_block(Block *s_block);

/**
 * @brief minero actualiza su cadena con el nuevo bloque válido
 * y lo manda al monitor (en caso de que esté activo)
 * 
 * @param last_block cadena de bloques
 * @param s_block nuevo bloque válido (shm)
 * @param s_net_data netdata (shm)
 * @param queue cola de mensajes
 * @param winner 1 si el minero es ganador, 0 si no
 * @param time_out tomará valor 1 si hay fallo por espera agotada
 * @return 1 en caso de error. 0 en caso contrario
 */
int mrr_valid_block_update(Block **last_block, Block* s_block, NetData *s_net_data, mqd_t queue, int winner, int* time_out);

/**
 * @brief minero está en su última ronda, quita su pid del array
 * de pids de mineros.
 * Si es ganador, elige otro ganador para que prepare la siguiente ronda.
 * 
 * @param mutex mutex que protege el acceso a la memoria
 * @param s_net_data netdata (shm)
 * @param this_index índice de este minero en los arrays compartidos
 */
void mrr_last_round(sem_t *mutex, NetData* s_net_data, int this_index);

/**
 * @brief si es el primer minero en salir del bucle de rondas por 
 * time_out, recontar mineros activos (quorum) y actualizar el campo
 * num_active_miners de la red
 * 
 * @param mutex mutex compartido
 * @param s_net_data netdata (shm)
 */
void mrr_fix_net(sem_t* mutex, NetData* s_net_data);


/* * * * * T R A B A J A D O R E S * * * * */
/**
 * @brief estructura para los hilos de los trabajadores
 * 
 */
typedef struct WorkerStruct_{
    long int target;
    long int begin; // primer valor que probar
    long int end; // último valor que probar
} WorkerStruct;

/**
 * @brief función para el hilo del trabajador. 
 * Solución al target en la estructura empezando por 
 * el valor inicial y terminando en el final que pone en su 
 * estructura pasada por d.
 * Si encuentra solución, lo comunica a los demás trabajadores 
 * poniendo la variable global end_threads a 1 y la variable
 * proof_solution con el valor de la solución. Y manda SIGHUP
 * al propio proceso para señalizar que es ganador.
 * 
 * @param d estructura de tipo WorkerStruct 
 * @return NULL en todo caso
 */
void *mrw_thread_mine(void *d);

/**
 * @brief inicializa n_workers estructuras de tipo WorkerStruct
 * en un array, alocando memoria para ello y estableciendo 
 * los valores iniciales y final de cada trabajador
 * 
 * @param n_workers número de trabajadores
 * @return WorkerStruct* el array reservado e inicializado
 */
WorkerStruct *mrw_struct_init(int n_workers);

/**
 * @brief lanza los hilos de los trabajadores
 * 
 * @param workers array de los hilos que se lanzan
 * @param workers_struct_arr array de las estructuras de los mineros
 * @param n_workers número de trabajadores
 * @param target nuevo target
 * @return EXIT_SUCCESS en caso de éxito, EXIT_FAILURE si hay algún error 
 */
int mrw_launch(pthread_t *workers, WorkerStruct *mine_struct, int nWorkers, long int target);

/**
 * @brief Hace join a todos los hilos trabajadores
 * 
 * @param workers array con los hilos
 * @param n_workers número de trabajadores
 */
void mrw_join(pthread_t* workers, int n_workers);

#endif