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

typedef struct Mine_struct{
    long int target;
    long int begin;
    long int end;
}Mine_struct;


/*workers*/
Mine_struct *mr_mine_struct_init(int n_workers);
void *mine(void *d);
void mr_workers_cancel(pthread_t *workers, int n_workers);
int mr_workers_launch(pthread_t *workers, Mine_struct *mine_struct,int nWorkers,long int target);


void handler(int sig);

Block *mr_shm_block_copy(Block *shm_b, Block *last_block);

void mr_shm_set_new_round(Block *b, NetData *d);

void mr_shm_set_solution(Block *b, long int solution);

int mr_shm_init(Block **b, NetData **d, int *this_index);

int mr_shm_map(char* file_name, void **p, size_t size);

void mr_shm_quorum(NetData *net);

void mr_blocks_free(Block* last_block);

int mr_miner_set_handlers(sigset_t mask);

void mr_masks_set_up(sigset_t *mask, sigset_t *mask_sigusr1, sigset_t *mask_sigusr2, sigset_t *old_mask);


#endif