#include "miner.h"
#include "mr_miner.h"
#include "mr_util.h"
#include <stdint.h>



int mr_shm_init_miner(Block **b, NetData **d, int *this_index)
{
    int created, i;
    pid_t this_pid = getpid();
    srand(time(NULL));

    created = mr_shm_map(SHM_NAME_BLOCK, (void**)b, sizeof(Block));

    if(created == MR_SHM_FAILED)
        return EXIT_FAILURE;
    else if(created == MR_SHM_CREATED)
    {
        for(i = 0; i < MAX_MINERS; i++)
            (*b)->wallets[i] = 0;
        (*b)->id = -1;
        (*b)->solution = rand()%PRIME;
        (*b)->is_valid = 0;
    }

    created = mr_shm_map(SHM_NAME_NET, (void**)d, sizeof(NetData));

    if(created == MR_SHM_FAILED)
    {
        munmap(*b, sizeof(**b));
        return EXIT_FAILURE;
    }// if just created or the creator was the monitor:
    if(created == MR_SHM_CREATED || ((*d)->last_miner == -1))
    {   
        for(i = 0; i < MAX_MINERS; i++)
        {
            (*d)->miners_pid[i] = -2;
            (*d)->voting_pool[i] = VOTE_NOT_VOTED;
        }
        if(created == MR_SHM_CREATED)
            (*d)->monitor_pid = -2;
        (*d)->miners_pid[0] = this_pid;
        (*d)->last_miner = 0;
        (*d)->last_winner = this_pid;
        (*d)->num_active_miners = 1;

        if(sem_init(&((*d)->sem_round_begin), 1, 0) != 0)
        {
            perror("sem_init");
            munmap(*b, sizeof(**b));
            return EXIT_FAILURE;
        }

        if (sem_init(&((*d)->sem_round_end), 1, 1) != 0)
        {
            perror("sem_init");
            munmap(*b, sizeof(**b));
            sem_destroy(&((*d)->sem_round_begin));
            munmap(*d, sizeof(**d));
            return EXIT_FAILURE;
        }

        if(sem_init(&((*d)->sem_scrutinizing), 1, 0) != 0)
        {
            perror("sem_init");
            munmap(*b, sizeof(**b));
            sem_destroy(&((*d)->sem_round_begin));
            sem_destroy(&((*d)->sem_round_end));
            munmap(*d, sizeof(**d));
            return EXIT_FAILURE;
        }

        if(sem_init(&((*d)->sem_votation_done), 1, 0) != 0)
        {
            perror("sem_init");
            munmap(*b, sizeof(**b));
            sem_destroy(&((*d)->sem_round_begin));
            sem_destroy(&((*d)->sem_round_end));
            sem_destroy(&((*d)->sem_scrutinizing));
            munmap(*d, sizeof(**d));
            return EXIT_FAILURE;
        }
    }
    else
    {   
        (*d)->last_miner++;
        (*d)->miners_pid[(*d)->last_miner] = this_pid;
        ((*d)->num_active_miners) ++;
    }

    (*this_index) = (*d)->last_miner; 

    return EXIT_SUCCESS;
}

void mr_shm_set_new_round(Block *b, NetData *d){
    int i;
    
    b->target = b->solution;
    b->id++;
    b->is_valid = 0;
    b->solution = -1;

    for(i = 0; i < d->last_miner; i++)
        d->voting_pool[i] = VOTE_NOT_VOTED;

    mr_shm_quorum(d);
}


void mr_shm_quorum(NetData *net){
    int n = 1, i;
    pid_t this_pid = getpid(),pid;

    for(i = 0 ; i <= (net->last_miner); i++){
        pid = net->miners_pid[i];
        if((this_pid != pid) && (pid > 0))
            n += (!(kill(pid, SIGUSR1)));
    }
    
    net->total_miners = n;
}


void mr_shm_set_solution(Block *b, long int solution){
    b->solution = solution;
}

void mr_masks_set_up(sigset_t *mask, sigset_t *mask_sigusr1, sigset_t *mask_sigusr2, sigset_t *old_mask)
{
    sigemptyset(mask);
    sigaddset(mask, SIGUSR1);
    sigaddset(mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, mask, old_mask);

    (*mask_sigusr1) = (*mask);
    sigdelset(mask_sigusr1, SIGUSR1);

    (*mask_sigusr2) = (*mask);
    sigdelset(mask_sigusr2, SIGUSR2);
}



int mr_check_votes(NetData *net){
    int i, count;
    char vote;

    for(i = count = 0; i <= (net->last_miner) ; i++){
        vote = net->voting_pool[i];
        count += (vote == VOTE_YES) - (vote == VOTE_NO);
    }

    return (count >= 0);
}

void mr_vote(NetData *net, Block *b, int index){
    int flag = (b->target == simple_hash(b->solution));
    
    net->voting_pool[index] = flag ? VOTE_YES : VOTE_NO;
    
    (net->total_miners)--;
    
    //El ultimo minero avisa al ganador (el ganador no vota)
    if(net->total_miners == 1) 
        sem_post(&(net->sem_votation_done));
}

void mr_send_end_scrutinizing(NetData *net, int n)
{
    while(n--) 
        sem_post(&(net->sem_scrutinizing));
}

void mr_lightswitchoff(sem_t *mutex, int *count, sem_t *sem)
{
    while (sem_wait(mutex) == -1);
    
    (*count)--;    
    if(!(*count))
        sem_post(sem);
    
    sem_post(mutex);
}


void mr_print_chain_file(Block *last_block, int n_wallets)
{
    char file_name[1024];
    int fd;

    sprintf(file_name, "%jd.log", (intmax_t)getpid());

    if ((fd = open(file_name, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1)
    {
        perror("open");
        return;
    }

    print_blocks_file(last_block, n_wallets, fd);
}

void mr_notice_miners(NetData *net)
{
    int i;
    pid_t this_pid=getpid(), pid;

    for(i=0;i<net->last_miner;i++){
        pid = net->miners_pid[i];
        if ((this_pid != pid) && (pid > 0))
            kill(net->miners_pid[i], SIGUSR2);
    }
}