/**
 * @file mr_util.h
 * @author your name (you@domain.com)
 * @brief Funciones comunes a mr_miner.c y mr_monitor.c
 * 
 * @date 2021-04-25
 * 
 * @copyright Copyright (c) 2021
 * 
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

int mr_shm_map(char* file_name, void **p, size_t size);

mqd_t mr_monitor_mq_open(char *queue_name, int __oflag);


void mr_blocks_free(Block *last_block);

Block* mr_shm_block_copy(Block *shm_b, Block *last_block);

#endif