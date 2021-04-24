#ifndef MINER_H
#define MINER_H

#include <unistd.h>
#include <sys/types.h>

#define OK 0
#define MAX_WORKERS 10

#define SHM_NAME_NET "/netdata"
#define SHM_NAME_BLOCK "/block"

#define PRIME 99997669
#define BIG_X 435679812
#define BIG_Y 100001819

#define MAX_MINERS 200


#define MR_SHM_FAILED -1
#define MR_SHM_CREATED 1
#define MR_SHM_EXISTS 0

typedef struct _Block {
    int wallets[MAX_MINERS];
    long int target;
    long int solution;
    int id;
    int is_valid;
    struct _Block *next;
    struct _Block *prev;
} Block;

typedef struct _NetData {
    pid_t miners_pid[MAX_MINERS];
    char voting_pool[MAX_MINERS];
    int last_miner;
    int total_miners;
    pid_t monitor_pid;
    pid_t last_winner;
} NetData;

long int simple_hash(long int number);

void print_blocks(Block * plast_block, int num_wallets);

/*******************************************************/


#endif