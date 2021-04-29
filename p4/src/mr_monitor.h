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
#define LOG_FILE "log.txt"
#define PRINTER_WAIT 5

typedef struct _monitor_blocks{
    Block buffer[MAX_BLOCKS];
    int last_block;
}Monitor_blocks;

void handler_sigint(int sig);

void handler_sigalrm(int sig);


/**
 * @brief hijo
 * 
 * @param fd 
 */
void mr_monitor_printer(int fd[2]);

void mr_monitor_printer_print_blocks(Block *last_block, int file);

int mr_shm_init_monitor(NetData **d);

/**
 * @brief 
 * 
 * @param block 
 * @param queue 
 * @return int 1 in case of error. 0 otherwise. 
 */
int mr_mq_receive(Block *block, mqd_t queue);

/**
 * @brief Comprueba si el bloque b está ya en blocks. 
 * Si está, comprueba que la target y la solución coincida, y 
 * devuelve 1. Si no está, lo añade y devuelve 0. En caso de error, 
 * *err se pone a 1 y se devuelve 1;
 * 
 * @param b 
 * @param blocks 
 * @param err puntero cuyo valor se establece a 1 en caso de error
 * @return int 0 si b no está en blocks y se añade con éxito.
 * 1 en caso contrario (o error). 
 * En caso de error, *err se pone a 1. 
 */
int mr_monitor_block_is_repeated(Block *b, Monitor_blocks *blocks, int *err);

/**
 * @brief 
 * 
 * @param block 
 * @param fd 
 * @return int 
 */
int mr_fd_read_block(Block *block, int fd[2]);

int mr_fd_write_block(Block *block, int fd[2]);

#endif