#include "miner.h"
#include "mr_miner.h"
#include "mr_util.h"

#define SEM_MUTEX_NAME2 "/sem2"

int winner = 0;
long int proof_solution;

static volatile sig_atomic_t got_sigusr1 = 0;
static volatile sig_atomic_t got_sigusr2 = 0;


void handler_miner(int sig)
{
    if (sig == SIGUSR1)
        got_sigusr1 = 1;
    else if (sig == SIGUSR2)
        got_sigusr2 = 1;
    
        
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
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
    int n_workers, n_rounds, err = 0, last_winner, this_index, n_voters;
    int time_out,i;
    pid_t this_pid = getpid();
    pthread_t *workers = NULL;
    Block *s_block, *last_block = NULL;
    NetData *s_net_data;
    Mine_struct *mine_struct = NULL;
    sem_t *mutex,*start_vote;
    sigset_t mask_sigusr1, mask_sigusr2, mask, old_mask;
    mqd_t queue;

    /*inicializar las máscaras y hacer sigprocmask*/
    mr_masks_set_up(&mask, &mask_sigusr1, &mask_sigusr2, &old_mask);

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
        exit(EXIT_FAILURE);
    }

    if (!(mine_struct = mr_mine_struct_init(n_workers)))
    {
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

    if ((start_vote = sem_open(SEM_MUTEX_NAME2, O_CREAT, S_IRUSR | S_IWUSR, 0)) == SEM_FAILED)
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

    //BUCLE DE RONDAS DE MINADO

    while (n_rounds-- && !err)
    {  
        //if (mr_timed_wait(&(s_net_data->sem_round_end), 3, &err, &time_out) == EXIT_FAILURE)
            //break;
        //sem_post(&(s_net_data->sem_round_end));

        winner = 0;
        last_winner = (this_pid == s_net_data->last_winner);

        if (last_winner)
        {
            //Preparar bloque y contar numero de mineros
            while (sem_wait(mutex) == -1);
            mr_shm_set_new_round(s_block, s_net_data);
            n_voters = s_net_data->total_miners - 1;
            
            //Avisar de que se inicia la
            for(i=0;i<n_voters;i++){
                printf("post round begin bloque %i con %i",s_block->id,n_voters);
                sem_post(&(s_net_data->sem_round_begin));
            }
            sem_post(mutex);
        }
        else
        {
            printf("Perdedor espera ronda nueva: \n");
            //Esperar a que empiece la ronda. RECIBIR SIGSUSPEND 1
            if (mr_timed_wait(&(s_net_data->sem_round_begin), 3, &err, &time_out) == EXIT_FAILURE)
            {
                break;
            } 
        }

        //LANZAR TRABAJADORES
        err = mr_workers_launch(workers, mine_struct, n_workers, s_block->target);
        if (err) break;

        //Esperar a conseguir la solucion o a q la consiga otro
        sigsuspend(&mask_sigusr2);

        //Matar trabajdores
        mr_workers_cancel(workers, n_workers);
        
        if (winner)
        {
            
            //Por si dos mineros se han declarado ganador
            while (sem_wait(mutex) == -1);
            printf("Posible ganador: %d bloque %i\n", this_pid, s_block->id);
            winner = (s_block->solution == -1);// 1 ssi eres el primero
            if(winner)
                mr_shm_set_solution(s_block, proof_solution);
            sem_post(mutex);

            if(winner)
            {
                
                while (sem_wait(mutex) == -1);
                printf("Verdadero ganador: %d bloque %i con sol: %ld y target %ld \n", this_pid, s_block->id,s_block->solution,s_block->target);
                //n_voters = (s_net_data->total_miners) - 1;

                mr_notice_miners(s_net_data);//sigusr2
                for(i=0;i<n_voters;i++){
                    sem_post(start_vote);
                }
                printf("Nvoters es %i\n", n_voters);
                sem_post(mutex);
               
                if(n_voters)
                {   
                    printf("Espero  a votacion\n");
                    while(sem_wait(&(s_net_data->sem_votation_done)) == -1);

                    if(mr_check_votes(s_net_data))
                    {
                        s_block->is_valid = 1;
                        s_net_data->last_winner = this_pid;
                        (s_block->wallets[this_index])++;
                    }
                    else
                    {
                        printf("shame\n"); //Elegir nuveo fake_last winner y comenzar nueva ronda
                    }

                    while (sem_wait(mutex) == -1);
                    //s_net_data->total_miners = n_voters + 1;
                    sem_post(mutex);

                    sem_wait(&(s_net_data->sem_round_end));//

                    while (sem_wait(mutex) == -1);
                    mr_send_end_scrutinizing(s_net_data, n_voters);
                    sem_post(mutex);
                }
                else
                {   
                    printf("No ha habido votacion bloque %i \n",s_block->id);
                    s_block->is_valid = 1;
                    s_net_data->last_winner = this_pid;
                    (s_block->wallets[this_index])++;
                }
            }
            else
            {
                printf("Falso ganador: %d bloque %i\n", this_pid, s_block->id);
                sigsuspend(&mask_sigusr2);
                printf("Falso ganador sale: %d bloque %i\n", this_pid, s_block->id);
            }
        }

        if(!winner)
        {
            sem_getvalue(&(s_net_data->sem_votation_done), &i);
            printf("Valor antes d votation sem es %i \n", i);

            sem_wait(start_vote);
            while(sem_wait(mutex) == -1);
            printf("Numero d mineros %i", s_net_data->total_miners);
            printf("Perdedor: %d va a votar bloque %i \n", this_pid, s_block->id);
            mr_vote(s_net_data, s_block, this_index);
            sem_post(mutex);

            printf("Ya he votado\n");
            while(sem_wait(&(s_net_data->sem_scrutinizing))==-1);
            
        }

        if(s_block->is_valid){
            //Añadir bloque correcto a la cadena de cada minero
            last_block = mr_shm_block_copy(s_block, last_block);

            if (last_block == NULL)
                err = 1;

            if (s_net_data->monitor_pid > 0 && (mq_send(queue, (char *)last_block, sizeof(Block), 1 + winner) == -1))
            {
                perror("mq_send");
                err = 1;
            }
        }

        if(!n_rounds) //Si es la ultima ronda, te das de baja
        {
            s_net_data->miners_pid[this_index] = -2;
        }
        

        mr_lightswitchoff(mutex, &(s_net_data->total_miners), &(s_net_data->sem_round_end));

        printf("pid: %d round: %d\n\n", this_pid, n_rounds);
    }

    mr_print_chain_file(last_block, s_net_data->last_miner + 1);

    mr_blocks_free(last_block);
    mq_close(queue);
    munmap(s_block, sizeof(Block));

    

    while(sem_wait(mutex) == -1);
    (s_net_data->num_active_miners)--;

    if(!(s_net_data->num_active_miners))
    {
        shm_unlink(SHM_NAME_BLOCK); //MIRAR!!!!!!!!!
        shm_unlink(SHM_NAME_NET);
        sem_unlink(SEM_MUTEX_NAME);
        mq_unlink(MQ_MONITOR_NAME);
    }
    sem_post(mutex);

    sem_close(mutex);
    munmap(s_net_data, sizeof(NetData));

    exit(EXIT_SUCCESS);
}