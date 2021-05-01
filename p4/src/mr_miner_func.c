#include "miner.h"
#include "mr_miner.h"
#include "mr_util.h"
#include <stdint.h>


int mr_miner_set_handlers(sigset_t mask)
{
    struct sigaction act;

    act.sa_mask = mask;
    act.sa_flags = 0;
    act.sa_handler = SIG_IGN; /* ignorar SIGUSR1 y SIGUSR2*/
    if (sigaction(SIGUSR1, &act, NULL) < 0)
    {
        perror("sigaction SIGUSR1");
        return (EXIT_FAILURE);
    }
    if (sigaction(SIGUSR2, &act, NULL) < 0)
    {
        perror("sigaction SIGUSR2");
        return (EXIT_FAILURE);
    }

    act.sa_handler = handler_miner;
    if (sigaction(SIGHUP, &act, NULL) < 0)
    {
        perror("sigaction SIGHUPINT");
        return (EXIT_FAILURE);
    }    
    if (sigaction(SIGINT, &act, NULL) < 0)
    {
        perror("sigaction SIGINT");
        return (EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

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
    if(created == MR_SHM_CREATED || ((*d)->last_miner == -1) )
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
    }
    else
    {   
        (*d)->last_miner++;
        (*d)->miners_pid[(*d)->last_miner] = this_pid;
        ((*d)->num_active_miners) ++;
    }    

    (*this_index) = (*d)->last_miner; 

    if((*d)->num_active_miners == 1)
    {   
        /*el primer minero activo siempre reestablece los semáforos y se declara ganador*/
        (*d)->last_winner = this_pid;
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

        if (sem_init(&((*d)->sem_start_voting), 1, 0) != 0)
        {
            perror("sem_init");
            munmap(*b, sizeof(**b));
            sem_destroy(&((*d)->sem_votation_done));
            sem_destroy(&((*d)->sem_round_begin));
            sem_destroy(&((*d)->sem_round_end));
            sem_destroy(&((*d)->sem_scrutinizing));
            munmap(*d, sizeof(**d));
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

void mr_shm_set_new_round(Block *b, NetData *d){
    int i;
    
    b->target = b->solution;
    b->id++;
    b->is_valid = 0;
    b->solution = -1;

    for(i = 0; i <= d->last_miner; i++)
        d->voting_pool[i] = VOTE_NOT_VOTED;

    mr_shm_quorum(d);
}

void mr_shm_quorum(NetData *net)
{
    int n = 1, i;
    pid_t this_pid = getpid(), pid;

    for (i = 0; i <= (net->last_miner); i++)
    {
        pid = net->miners_pid[i];
        if ((this_pid != pid) && (pid > 0))
            n += (!(kill(pid, SIGUSR1)));
    }

    net->total_miners = n;
}

void mr_shm_set_solution(Block *b, long int solution){
    b->solution = solution;
}

void mr_masks_set_up(sigset_t *mask, sigset_t *mask_wait_workers, sigset_t *old_mask)
{
    sigemptyset(mask);
    sigaddset(mask, SIGUSR1);
    sigaddset(mask, SIGUSR2);
    sigaddset(mask, SIGHUP);
    sigaddset(mask, SIGINT);
    sigprocmask(SIG_BLOCK, mask, old_mask);

    /*la máscara para esperar a los trabajadores tiene solo SIGUSR1*/
    (*mask_wait_workers) = (*mask);
    sigdelset(mask_wait_workers, SIGUSR2);
    sigdelset(mask_wait_workers, SIGHUP);
}

int mr_check_votes(NetData *net)
{
    int i, count;
    char vote;

    for (i = count = 0; i <= (net->last_miner); i++)
    {
        vote = net->voting_pool[i];
        count += (vote == VOTE_YES);
    }
    
    return ((2 * count) >= net->total_miners);
}

void mr_vote(sem_t *mutex, NetData *net, Block *b, int index){
    int flag;
    
    flag = (b->target == simple_hash(b->solution));
    net->voting_pool[index] = flag ? VOTE_YES : VOTE_NO;
    
    mr_lightswitchoff(mutex, &(net->num_voters), &(net->sem_votation_done));

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

void mr_notify_miners(NetData *net)
{
    int i;
    pid_t this_pid = getpid(), pid;

    for (i = 0; i <= net->last_miner; i++)
    {
        pid = net->miners_pid[i];
        if ((this_pid != pid) && (pid > 0))
            kill(pid, SIGUSR2);
    }
}


void mr_last_winner_prepare_round(Block* s_block, NetData* s_net_data)
{
    int n_proc, i;
    /*Preparar bloque y contar numero de mineros*/
    mr_shm_set_new_round(s_block, s_net_data);
    n_proc = s_net_data->total_miners - 1;

    //Avisar de que se inicia la ronda
    for (i = 0; i < n_proc; i++)
    {
        sem_post(&(s_net_data->sem_round_begin));
    }
}

int mr_real_winner_actions(Block* s_block, NetData* s_net_data, int this_index)
{
    int n_voters, i;

    
    n_voters = (s_net_data->total_miners) - 1;
    s_net_data->num_voters = n_voters;

    mr_notify_miners(s_net_data);//mandar sigusr2

    for (i = 0; i < n_voters; i++)
    {
        sem_post(&(s_net_data->sem_start_voting));
    }

    if (!n_voters)
        mr_winner_update_after_votation(s_block, s_net_data, this_index);
    else
    {
        
        if ((mr_timed_wait(&(s_net_data->sem_votation_done), 3)))
            return 1;

        if (mr_check_votes(s_net_data))
        {
            mr_winner_update_after_votation(s_block, s_net_data, this_index);
        }
        else
        {               
            mr_miner_loser_modify_block(s_block);
            printf("shame\n"); //Elegir nuevo fake_last winner y comenzar nueva ronda
        }
    }

    // para que no entren mineros en el bucle hasta que el último haya terminado:
    if ((mr_timed_wait(&(s_net_data->sem_round_end), 3)))
            return 1;

    if (n_voters)
        mr_send_end_scrutinizing(s_net_data, n_voters);
    
    return 0;
}

void mr_winner_update_after_votation(Block* s_block, NetData* s_net_data, int this_index)
{
    s_block->is_valid = 1;
    s_net_data->last_winner = getpid();
    (s_block->wallets[this_index])++;
}

void mr_miner_close_net_mutex(sem_t *mutex, NetData* s_net_data)
{
    while(sem_wait(mutex) == -1);
    (s_net_data->num_active_miners)--;
    
    if(!(s_net_data->num_active_miners))
    {   /*el último minero destruye los semáforos*/
        sem_destroy(&(s_net_data->sem_round_begin));
        sem_destroy(&(s_net_data->sem_round_end));
        sem_destroy(&(s_net_data->sem_scrutinizing));
        sem_destroy(&(s_net_data->sem_start_voting));
        sem_destroy(&(s_net_data->sem_votation_done));
        if (s_net_data->monitor_pid < 0)
        {   /*si no hay más mineros ni monitor, hacer unlink*/
            printf("destroy everything\n");
            sem_unlink(SEM_MUTEX_NAME);
            shm_unlink(SHM_NAME_BLOCK);
            shm_unlink(SHM_NAME_NET);
            sem_unlink(SEM_MUTEX_NAME);
            mq_unlink(MQ_MONITOR_NAME);
        }
    }
    sem_post(mutex);

    sem_close(mutex);
    munmap(s_net_data, sizeof(NetData));
}

int mr_valid_block_update(Block **last_block, Block* s_block, NetData *s_net_data, mqd_t queue, int winner)
{
    /*Añadir bloque correcto a la cadena de cada minero*/
    (*last_block) = mr_shm_block_copy(s_block, *last_block);

    if ((*last_block) == NULL)
        return 1;

    /*Si hay monitor, mandar el nuevo bloque*/
    if ((s_net_data->monitor_pid > 0) && 
        (mq_send(queue, (char *)(*last_block), sizeof(Block), 1 + winner) == -1))
    {
        perror("mq_send");
        return 1;
    }
    return 0;
}

void mr_miner_last_round(sem_t *mutex, NetData* s_net_data, int this_index)
{
    int i;

    while (sem_wait(mutex) == -1);

    s_net_data->miners_pid[this_index] = -2;
    if (getpid() == s_net_data->last_winner)
    {
        for (i = 0; i <= s_net_data->last_miner; i++)
        {
            if (s_net_data->miners_pid[i] > 0)
            {
                s_net_data->last_winner = s_net_data->miners_pid[i];
                break;
            }
        }
    }
    sem_post(mutex);
}


void mr_miner_loser_modify_block(Block* s_block)
{
    s_block->id--;
    s_block->solution = s_block->target;
}