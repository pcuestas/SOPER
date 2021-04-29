#include "miner.h"
#include "mr_miner.h"
#include "mr_util.h"


int end_threads = 0;
long int proof_solution;

static volatile sig_atomic_t got_sighup = 0;
static volatile sig_atomic_t got_sigint = 0;


void handler_miner(int sig)
{
    if (sig == SIGINT)
        got_sigint = 1;
    else if (sig == SIGHUP)
        got_sighup = 1;        
}

int mr_miner_set_handlers(sigset_t mask)
{
    struct sigaction act;

    act.sa_mask = mask;
    act.sa_flags = 0;
    act.sa_handler = handler_miner; // sig_ign para sigusr1 ? 

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

int main(int argc, char *argv[])
{
    int n_workers, n_rounds, err = 0, last_winner, this_index, winner;
    pid_t this_pid = getpid();
    pthread_t *workers = NULL;
    Block *s_block, *last_block = NULL;
    NetData *s_net_data;
    Mine_struct *mine_struct = NULL;
    sem_t *mutex;
    sigset_t mask_wait_workers, mask, old_mask;
    mqd_t queue;

    /*inicializar las máscaras y hacer sigprocmask*/
    mr_masks_set_up(&mask, &mask_wait_workers, &old_mask);

    if (argc != 3)
    {
        fprintf(stderr, "Uso: %s <Número_de_trabajadores> <Número_de_rondas>\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    n_workers = atoi(argv[1]);
    n_rounds = atoi(argv[2]);

    if (mr_miner_set_handlers(mask) == EXIT_FAILURE)
    {
        exit(EXIT_FAILURE);
    }

    if (!(workers = (pthread_t *)calloc(n_workers, sizeof(pthread_t))))
    {
        perror("calloc");
        exit(EXIT_FAILURE);
    }

    if (!(mine_struct = mr_mine_struct_init(n_workers)))
    {
        perror("calloc");
        free(workers);
        exit(EXIT_FAILURE);
    }

    if ((mutex = sem_open(SEM_MUTEX_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED)
    {
        perror("sem_open");
        free(workers);
        free(mine_struct);
        exit(EXIT_FAILURE);
    }

    queue = mr_monitor_mq_open(MQ_MONITOR_NAME, O_CREAT | O_WRONLY);
    if (queue == (mqd_t)-1)
    {
        free(workers);
        free(mine_struct);
        sem_close(mutex);
        exit(EXIT_FAILURE);
    }

    //Registrar minero en la red
    while (sem_wait(mutex) == -1);    
    if (mr_shm_init_miner(&s_block, &s_net_data, &this_index) == EXIT_FAILURE)
    {
        free(mine_struct);
        free(workers);
        exit(EXIT_FAILURE);
    }
    sem_post(mutex);
    printf("my index:%d\n", this_index);

    //BUCLE DE RONDAS DE MINADO

    while (n_rounds-- && !err && !got_sigint)
    { 
        if ((err = mr_timed_wait(&(s_net_data->sem_round_end), 3)))
            break;
        sem_post(&(s_net_data->sem_round_end));
        
        winner = 0;
        while(sem_wait(mutex) == -1);
        last_winner = (this_pid == s_net_data->last_winner);
        sem_post(mutex);

        if (last_winner)
        {
            mr_last_winner_prepare_round(mutex, s_block, s_net_data);
        }
        else
        {
            //Esperar a que empiece la ronda. RECIBIR SIGSUSPEND 1
            if ((err = mr_timed_wait(&(s_net_data->sem_round_begin), 3)))
                break;
        }

        //lanzar trabajadores
        if ((err = mr_workers_launch(workers, mine_struct, n_workers, s_block->target))) 
            break;

        //Esperar a conseguir la solución o a que la consiga otro
        sigsuspend(&mask_wait_workers);
        
        //mr_workers_cancel(workers, n_workers);//matar trabajdores
        end_threads = 1;

        if (got_sighup) /*los trabajadores de este proceso han encontrado solución*/
        {
            got_sighup = 0;
            //Por si dos mineros se han declarado ganador:
            while (sem_wait(mutex) == -1);
            winner = (s_block->solution == -1);// 1 ssi es el primero
            if(winner)
                mr_shm_set_solution(s_block, proof_solution);
            sem_post(mutex);

            if(winner)
            {               
                mr_real_winner_actions(mutex, s_block, s_net_data, this_index);
            }
        }
        if(!winner)
        {
            while (sem_wait(&(s_net_data->sem_start_voting)) == -1);
            mr_vote(mutex, s_net_data, s_block, this_index);
            while (sem_wait(&(s_net_data->sem_scrutinizing)) == -1);            
        }

        if(s_block->is_valid){
            err = mr_valid_block_update(&last_block, s_block, s_net_data, queue, winner);
        }
        
        sigprocmask(SIG_SETMASK, &old_mask, &mask); // receive all signals remaining
        sigprocmask(SIG_SETMASK, &mask, &old_mask); // restore mask

        if(!n_rounds || got_sigint)
        {//Si es su última ronda, se das de baja
            mr_miner_last_round(mutex, s_net_data, this_index);
        }    
        
        mr_lightswitchoff(mutex, &(s_net_data->total_miners), &(s_net_data->sem_round_end));

        printf("pid: %d rounds remaining: %d\n\n", this_pid, n_rounds);
    }

    mr_print_chain_file(last_block, s_net_data->last_miner + 1);

    free(workers);
    free(mine_struct);
    mr_blocks_free(last_block);
    mq_close(queue);
    munmap(s_block, sizeof(Block));

    mr_close_net_mutex(mutex, s_net_data);

    exit(EXIT_SUCCESS);
}