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

typedef struct Mine_struct
{
    long int target;
    long int begin;
    long int end;

} Mine_struct;

/*workers*/
Mine_struct *mr_mine_struct_init(int n_workers);
void *mine(void *d);
int mr_workers_launch(pthread_t *workers, Mine_struct *mine_struct, int nWorkers, long int target);

void mr_workers_cancel(pthread_t* workers, int n_workers);

void handler_miner(int sig);

void mr_shm_set_new_round(Block *b, NetData *d);

void mr_shm_set_solution(Block *b, long int solution);

int mr_shm_init_miner(Block **b, NetData **d, int *this_index);

int mr_shm_map(char *file_name, void **p, size_t size);

void mr_shm_quorum(NetData *net);

int mr_miner_set_handlers(sigset_t mask);

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

#endif