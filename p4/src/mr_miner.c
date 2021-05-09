/**
 * @file mr_miner.c (Proyecto SOPER)
 * @author Pablo Cuesta Sierra, Álvaro Zamanillo Sáez
 * @brief main del minero.
 */
#include "miner.h"
#include "mr_miner.h"
#include "mr_util.h"

int end_threads = 0;
long int proof_solution;

static volatile sig_atomic_t got_sighup = 0;
static volatile sig_atomic_t got_sigint = 0;

/**
 * @brief manejador de señales del minero
 * 
 * @param sig 
 */
void handler_miner(int sig)
{
    if (sig == SIGINT)
        got_sigint = 1;
    else if (sig == SIGHUP)
        got_sighup = 1;        
}

int main(int argc, char *argv[])
{
    int n_workers, n_rounds, err = 0, this_index, winner, time_out;
    pid_t this_pid = getpid();
    Block *s_block, *last_block = NULL;
    pthread_t *workers = NULL;
    NetData *s_net_data;
    WorkerStruct *mine_struct = NULL;
    sem_t *mutex;
    sigset_t mask_wait_workers, mask, old_mask;
    mqd_t queue;

    /*inicializar las máscaras y hacer sigprocmask*/
    mrr_masks_set_up(&mask, &mask_wait_workers, &old_mask);

    if ((argc != 3) || ((n_workers = atoi(argv[1])) <= 0))
    {
        fprintf(stderr, "Uso: %s <Número_de_trabajadores> <Número_de_rondas>\n<Número_de_trabajadores> > 0\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    n_rounds = atoi(argv[2]);    
    if(!n_rounds)      /*para que si tiene n_rounds == 0, */
        n_rounds-- ;   /*continue hasta que se le pare    */

    if (mrr_set_handlers(mask) == EXIT_FAILURE)
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
    if (mrr_shm_init(&s_block, &s_net_data, &this_index) == EXIT_FAILURE)
    {
        free(workers);
        free(mine_struct);
        sem_post(mutex);
        sem_close(mutex);
        exit(EXIT_FAILURE);
    }
    sem_post(mutex);

    /*BUCLE DE RONDAS DE MINADO*/
    while (n_rounds-- && !err && !got_sigint)
    { 
        if ((err = mr_timed_wait(&(s_net_data->sem_round_end), 3, &time_out)))
            break;
        sem_post(&(s_net_data->sem_round_end));
        
        winner = 0;

        if ((this_pid == s_net_data->last_winner))
        {   /*el último ganador, prepara la ronda*/
            mrr_last_winner_prepare_round(s_block, s_net_data);
        }
        else
        {   /*esperar a que el anterior ganador prepare la ronda*/
            if ((err = mr_timed_wait(&(s_net_data->sem_round_begin), 3, &time_out)))
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
                mrr_set_solution(s_block, proof_solution);
            sem_post(mutex);

            if(winner && 
              (time_out = mrr_winner_actions(s_block, s_net_data, this_index)))
                break;
        }
        if (!winner)
        {   /*los perdedores de la ronda*/
            if ((err = mr_timed_wait(&(s_net_data->sem_start_voting), 3, &time_out)))
                break;
            mrr_vote(mutex, s_net_data, s_block, this_index);
            if ((err = mr_timed_wait(&(s_net_data->sem_scrutiny), 3, &time_out)))
                break;           
        }

        if(s_block->is_valid)
        {
            if((err = mrr_valid_block_update(&last_block, s_block, s_net_data, queue, winner,&time_out)))
                break;
        }          
        else
            n_rounds++;/*una ronda con votación fallida no cuenta*/
        
        sigprocmask(SIG_SETMASK, &old_mask, &mask); /*recibir las señales pendientes (posiblemente SIGINT)*/ 
        sigprocmask(SIG_SETMASK, &mask, &old_mask); /*reestablecer la máscara*/

        if(!n_rounds || got_sigint) /*si es su última ronda, se da de baja*/
            mrr_last_round(mutex, s_net_data, this_index);
        
        mr_lightswitchoff(mutex, &(s_net_data->total_miners), &(s_net_data->sem_round_end));

        printf("miner:%d-remaining rounds:%d\n", this_pid, n_rounds);
    }

    if(time_out)
        mrr_fix_net(mutex, s_net_data);    

    mrr_print_chain(last_block, s_net_data->last_miner + 1);

    free(mine_struct);
    free(workers);
    mr_blocks_free(last_block);
    mq_close(queue);
    munmap(s_block, sizeof(Block));

    mrr_close_net_mutex(mutex, s_net_data);

    exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
}
