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

int main(int argc, char *argv[])
{
    int n_workers, n_rounds, err = 0, this_index, winner;
    pid_t this_pid = getpid();
    Block *s_block, *last_block = NULL;
    pthread_t *workers = NULL;
    NetData *s_net_data;
    WorkerStruct *mine_struct = NULL;
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

    if (!(mine_struct = mrw_struct_init(n_workers)))
    {
        exit(EXIT_FAILURE);
    }

    if (!(workers = (pthread_t*)calloc(n_workers, sizeof(pthread_t))))
    {
        perror("calloc");
        free(mine_struct);
        exit(EXIT_FAILURE);
    }

    if ((mutex = sem_open(SEM_MUTEX_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED)
    {
        perror("sem_open");
        free(workers);
        free(mine_struct);
        exit(EXIT_FAILURE);
    }

    queue = mr_mq_open(MQ_MONITOR_NAME, O_CREAT | O_WRONLY);
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
        free(workers);
        free(mine_struct);
        sem_post(mutex);
        sem_close(mutex);
        exit(EXIT_FAILURE);
    }
    sem_post(mutex);

    if(!n_rounds) n_rounds-- ;

    /*BUCLE DE RONDAS DE MINADO*/
    while (n_rounds-- && !err && !got_sigint)
    { 
        if ((err = mr_timed_wait(&(s_net_data->sem_round_end), 3)))
            break;
        sem_post(&(s_net_data->sem_round_end));
        
        winner = 0;

        if ((this_pid == s_net_data->last_winner))
        {   /*el último ganador, prepara la ronda*/
            mr_last_winner_prepare_round(s_block, s_net_data);
        }
        else
        {   /*esperar a que el anterior ganador prepare la ronda*/
            if ((err = mr_timed_wait(&(s_net_data->sem_round_begin), 3)))
                break;
        }

        /*lanzar trabajadores*/
        if ((err = mrw_launch(workers, mine_struct, n_workers, s_block->target))) 
            break;

        /*Esperar a conseguir la solución o a que la consiga otro*/
        sigsuspend(&mask_wait_workers);
        
        mrw_join(workers, n_workers);

        if (got_sighup) 
        {   /*los trabajadores de este proceso han encontrado solución*/
            got_sighup = 0;
            /*por si dos mineros se han declarado ganador:*/
            while (sem_wait(mutex) == -1);
            winner = (s_block->solution == -1);/* 1 ssi es el primero ("verdadero ganador")*/
            if(winner)
                mr_shm_set_solution(s_block, proof_solution);
            sem_post(mutex);

            if(winner && 
            (err = mr_real_winner_actions(s_block, s_net_data, this_index)))
                break;
        }
        if(!winner)
        {   /*los perdedores de la ronda*/
            if ((err = mr_timed_wait(&(s_net_data->sem_start_voting), 3)))
                break;
            mr_vote(mutex, s_net_data, s_block, this_index);
            if ((err = mr_timed_wait(&(s_net_data->sem_scrutinizing), 3)))
                break;           
        }

        if(s_block->is_valid)
            err = mr_valid_block_update(&last_block, s_block, s_net_data, queue, winner);
        else
            n_rounds++;/*una ronda con votación fallida no cuenta*/
        
        sigprocmask(SIG_SETMASK, &old_mask, &mask); /*recibir las señales restantes*/ 
        sigprocmask(SIG_SETMASK, &mask, &old_mask); /*reestablecer la máscara*/

        if(!n_rounds || got_sigint) /*si es su última ronda, se da de baja*/
            mr_miner_last_round(mutex, s_net_data, this_index);
        
        mr_lightswitchoff(mutex, &(s_net_data->total_miners), &(s_net_data->sem_round_end));
        printf("miner:%d-remaining rounds:%d\n", this_pid, n_rounds);
    }

    mr_print_chain_file(last_block, s_net_data->last_miner + 1);

    free(mine_struct);
    free(workers);
    mr_blocks_free(last_block);
    mq_close(queue);
    munmap(s_block, sizeof(Block));

    mr_miner_close_net_mutex(mutex, s_net_data);

    exit(EXIT_SUCCESS);
}