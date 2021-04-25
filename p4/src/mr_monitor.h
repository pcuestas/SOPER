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

void print_blocks_file(Block *plast_block, int num_wallets, int fd);


/**
 * @brief hijo
 * 
 * @param fd 
 */
void mr_monitor_printer(int fd[2]);

/**
 * @brief 
 * 
 * @param block 
 * @param queue 
 * @return int 1 in case of error. 0 otherwise. 
 */
int mr_mq_receive(Block *block, mqd_t queue);

/**
 * @brief 
 * 
 * @param block 
 * @param fd 
 * @return int 
 */
int mr_fd_read_block(Block *block, int fd[2]);

int block_is_repeated(Block *b, Monitor_blocks *blocks, int *err);

int mr_fd_write_block(Block *block, int fd[2]);

int mr_shm_init_monitor(NetData **d);

#endif