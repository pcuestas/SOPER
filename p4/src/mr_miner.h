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


#define SEM_MUTEX_NAME "/mr_mutex"

#define VOTE_YES 1
#define VOTE_NO 0
#define VOTE_NOT_VOTED 2

static volatile sig_atomic_t got_sigusr1 = 0;
static volatile sig_atomic_t got_sigusr2 = 0;

typedef struct Mine_struct{
    long int target;
    long int begin;
    long int end;
}Mine_struct;

void *mine(void *d);

void handler(int sig);

Mine_struct *mr_mine_struct_init(int n_workers);

int mr_workers_launch(pthread_t *workers, Mine_struct *mine_struct,int nWorkers,long int target);

void mr_workers_cancel(pthread_t *workers, int n_workers);

Block *mr_add_block(Block *b, Block *last_block);

void mr_set_block_new_round(Block *b, NetData *d);

int mr_init_shm(Block **b, NetData **d, int *this_index);

int mr_shm_map(char* file, void **p, size_t size);

int mr_set_miner_handlers(sigset_t mask);

void mr_free_blocks(Block* last_block);

void mr_quorum_update(NetData * net);


#endif