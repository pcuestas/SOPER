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

void mr_shm_set_solution(Block *b, long int solution);

int mr_shm_map(char *file_name, void **p, size_t size);

void mrr_quorum(NetData *net);

void mr_masks_set_up(sigset_t *mask, sigset_t *mask_wait_workers, sigset_t *old_mask);


int mr_check_votes(NetData *net);

void mr_vote(sem_t *mutex, NetData *net, Block *b, int index);

void mr_send_end_scrutinizing(NetData *net, int n);

void mr_lightswitchoff(sem_t *mutex, int *count, sem_t *sem);

void mr_notify_miners(NetData *net);

void mr_print_chain_file(Block *last_block, int n_wallets);



void mr_last_winner_prepare_round(Block* s_block, NetData* s_net_data);

int mr_real_winner_actions(Block* s_block, NetData* s_net_data, int this_index);

void mr_winner_update_after_votation(Block* s_block, NetData* s_net_data, int this_index);

void mr_miner_close_net_mutex(sem_t *mutex, NetData* s_net_data);

void mr_miner_loser_modify_block(Block *s_block);

/**
 * @brief 
 * 
 * @param last_block 
 * @param s_block 
 * @param s_net_data 
 * @param queue 
 * @param winner 
 * @return 1 iff error, else 0
 */
int mr_valid_block_update(Block **last_block, Block* s_block, NetData *s_net_data, mqd_t queue, int winner);


void mr_miner_last_round(sem_t *mutex, NetData* s_net_data, int this_index);

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
 * @return WorkerStruct* el array reservad ei inicializado
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