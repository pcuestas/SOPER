#include "miner.h"
#include "mr_miner.h"

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
    act.sa_handler = handler_miner;

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

/*
 Liberar:
    workers
    mine_struct
 */

mr_notice_miners(NetData *net){
    int i;
    pid_t this_pid=getpid();

    for(i=0;i<net->last_miner;i++){
        if (this_pid != net->miners_pid[i])
            kill(net->miners_pid[i], SIGUSR2);
    }
}

mr_vote(NetData *net, Block *b,int index){
    net->voting_pool[index]=(b->target==simple_hash(b->solution))?VOTE_YES:VOTE_NO;
    net->total_miners--;
    if(net->total_miners==0) //El ultimo minero avisa al ganador
        //sem_post(voting_done);
        return;
}

int main(int argc, char *argv[])
{
    int n_workers, n_rounds, err = 0, last_winner, this_index;
    pid_t this_pid = getpid();
    pthread_t *workers = NULL;
    Block *s_block, *last_block = NULL;
    NetData *s_net_data;
    Mine_struct *mine_struct = NULL;
    sem_t *mutex;
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

    queue = mr_monitor_mq_open(MQ_MONITOR_NAME, O_CREAT | O_WRONLY);
    if (queue == (mqd_t)-1)
    {
        free(workers);
        free(mine_struct);
        sem_close(mutex);
        exit(EXIT_FAILURE);
    }

    //Registrar minero en la red
    while (sem_wait(mutex) == -1)
        ;
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
        last_winner = (this_pid == s_net_data->last_winner);

        if (last_winner)
        {
            //Preparar bloque y contar numero de mineros
            mr_shm_set_new_round(s_block, s_net_data); 

            while (sem_wait(mutex) == -1);
            mr_shm_quorum(s_net_data);
            sem_post(mutex);
            
        }
        else
        {   
            //Esperar a que empiece la ronda. RECIBIR SIGSUSPEND 1
            sigsuspend(&mask_sigusr1);// prefiero semáforo
        }

        //LANZAR TRABAJADORES
        err = mr_workers_launch(workers, mine_struct, n_workers, s_block->target);
        if (err)
            break;

        //Esperar a conseguir la solucion o a q la consiga otro
        //alarm(3 segundos)
        sigsuspend(&mask_sigusr2) ; // ver si sales por sigalarm
        //if(gotalarm)
        //    exit;

        ////Matar a sus trabajdores
        //mr_workers_cancel(workers, n_workers);


        if (winner) //Que pasa si hay dos ganadores!!!, winner comprobar si ya hay gaandor en memoria compartida? posible solucion
        {
            // cargar_solucion
            mr_shm_set_solution(s_block, proof_solution);

            // avisar_mineros(); (mandar sigUsr2)
            while (sem_wait(mutex) == -1);
            mr_notice_miners(s_net_data);
            sem_post(mutex);

            //Matar a sus trabajdores
            mr_workers_cancel(workers, n_workers);

            // semáforo que dice que ha terminado la votación
            //sem_down(votación);

            //Esto con mutex
            // if (votación_favorable()){
                s_block->is_valid = 1;
                s_net_data->last_winner = this_pid;
                (s_block->wallets[this_index])++;
            //}
            //else{me voy}
            //up(votación_terminada, num_mineros)
        }

        // else{ PERDEDOR
        //     mr_vote(s_net_data,s_block,this_index);
        //      si soy último en votar--> up(votación)

        //   !!sigsuspend(&mask_sigusr2); //esperar a que termine votacion (por qué no semáforo ???? )
        //
        //     
        // }

        //Añadir bloque correcto a la cadena de cada minero
        last_block = mr_shm_block_copy(s_block, last_block);

        if (last_block == NULL)
            err = 1;

        if (winner)
        {
            winner = 0;
            last_winner = 1; //CAMBIAR SEGUN VOTOS ?!?!?!
        }

        if (s_net_data->monitor_pid > 0 && (mq_send(queue, (char *)last_block, sizeof(Block), 1 + winner) == -1))
        {
            perror("mq_send");
            err = 1;
        }
    }

    print_blocks(last_block, 1);
    mr_blocks_free(last_block);

    mq_close(queue);
    munmap(s_net_data, sizeof(NetData));
    munmap(s_block, sizeof(Block));

    shm_unlink(SHM_NAME_BLOCK); //MIRAR!!!!!!!!!
    shm_unlink(SHM_NAME_NET);
    sem_unlink(SEM_MUTEX_NAME);
    mq_unlink(MQ_MONITOR_NAME);

    exit(EXIT_SUCCESS);
}