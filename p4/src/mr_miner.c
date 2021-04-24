#include "miner.h"
#include "mr_miner.h"

int winner=0;
int proof_solution;

void *mine(void *d){
    Mine_struct *data = (Mine_struct*)d;
    long int i;
    for(i = 0 ; i < data->end && !winner ;i++){
        if(data->target == simple_hash(i)){
            proof_solution = i;
            kill(getpid(),SIGUSR2);
            winner = 1;
        }
    }

    return NULL;    
}


Mine_struct *mr_mine_struct_init(int n_workers){
    Mine_struct *mine_struct = (Mine_struct *)calloc(n_workers, sizeof(Mine_struct));
    int i;
    long int interval = PRIME / n_workers;
    
    if(mine_struct == NULL)
        return NULL;

    mine_struct[0].begin = 0;

    for(i = 1; i < n_workers; i++)
    {
        mine_struct[i-1].end = mine_struct[i].begin = i*interval;
    }
    mine_struct[n_workers - 1].end = PRIME - 1;

    return mine_struct;
}

int mr_workers_launch(pthread_t *workers, Mine_struct *mine_struct,int nWorkers,long int target){
    int i,j,err=0;
    for (i = 0; i < nWorkers && !err; i++){
        mine_struct[i].target = target;
        err = pthread_create(&workers[i], NULL, mine, (void *)&(mine_struct[i]));
    }

    if(err){
        for(j=0;j<i;j++){
            pthread_cancel(workers[j]);
        }
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

void mr_workers_cancel(pthread_t *workers, int n_workers){
    int j;
    for (j = 0; j < n_workers; j++){
        pthread_cancel(workers[j]);
    }
    
}

Block* mr_add_block(Block *b, Block *last_block){
    Block *new_block=NULL;

    if(!(new_block=(Block*)calloc(1,sizeof(Block)))){
        return NULL;
    }

    memcpy(new_block, b, sizeof(Block));
    last_block->next = new_block;
    new_block->prev = last_block;

    return new_block;
}

void mr_set_solution(Block *b, long int solution){
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

/*
 Liberar:
    workers
    mine_struct
 */



int main(int argc, char *argv[]){
    int n_workers, n_rounds, err = 0, last_winner;
    pid_t this_pid = getpid();
    pthread_t *workers = NULL;
    Block *s_block, *last_block = NULL;
    NetData *s_net_data;
    Mine_struct *mine_struct = NULL;
    sem_t *mutex;
    sigset_t mask_sigusr1, mask_sigusr2, mask, old_mask;


    /*inicializar las máscaras y hacer sigprocmask*/
    mr_masks_set_up(&mask, &mask_sigusr1, &mask_sigusr2, &old_mask);
    
    if (argc != 3)
    {
        fprintf(stderr, "Uso: %s <Número_de_trabajadores> <Número_de_rondas>\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    n_workers = atoi(argv[1]);
    n_rounds = atoi(argv[2]);

    if(mr_set_miner_handlers(mask) == EXIT_FAILURE)
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

    //Registrar minero en la red
    while(sem_wait(mutex) == -1);
    if (mr_init_shm(&s_block, &s_net_data) == EXIT_FAILURE)
    {
        free(mine_struct);
        free(workers);
        exit(EXIT_FAILURE);
    }
    sem_post(mutex);
/****/

    s_block->id = 2;
    printf("", s_block->solution);


    printf("callocs\n");
    exit(EXIT_FAILURE);

    //BUCLE DE RONDAS DE MINADO

    while(n_rounds-- && !err){
        last_winner = (this_pid == s_net_data->last_winner);

        if(last_winner){
            //Preparar bloque y contar numero de mineros
            mr_set_block_new_round(s_block, s_net_data);
            while(sem_wait(mutex) == -1);
            mr_quorum_update(s_net_data);
            sem_post(mutex);
        }
        else
        {
            sigsuspend(&mask_sigusr1);
        }
        

        //LANZAR TRABAJADORES
        err = mr_workers_launch(workers,mine_struct,n_workers,s_block->target);
        if(err) break;

        //Esperar a sigsuspend(SIGUSR2)
        sigsuspend(&mask_sigusr2);

        if(winner){
            // cargar_solucio
            mr_set_solution(s_block, proof_solution);

            // avisar_mineros(); (mandar sigUsr2)

            //Matar a sus trabajdores
            mr_workers_cancel(workers, n_workers);

            // semáforo que dice que ha terminado la votación
        }

        // else{
        //     res = comprobar_sol();
        //     votar(res) 
        //     si eres el último, up del terminado de votación
        // }

        //Esperar a que se haya votado
        
        //Añadir bloque correcto a la cadena de cada minero
        last_block = mr_add_block(s_block, last_block);
        if(last_block == NULL)
            err = 1;

        if(winner){
            winner = 0;
            last_winner = 1; //CAMBIAR SEGUN VOTOS !!!!!
        }
    }

    print_blocks(last_block,0);

    mr_free_blocks(last_block);

    exit(EXIT_SUCCESS);
}