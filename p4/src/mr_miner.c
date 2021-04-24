#include "miner.h"
#include "mr_miner.h"

int winner = 0;
long int proof_solution;

static volatile sig_atomic_t got_sigusr1 = 0;
static volatile sig_atomic_t got_sigusr2 = 0;

void handler(int sig){
    if(sig == SIGUSR1)
        got_sigusr1 = 1;
    else if(sig == SIGUSR2)
        got_sigusr2 = 1;
}

/*
 Liberar:
    workers
    mine_struct
 */

int main(int argc, char *argv[]){
    int n_workers, n_rounds, err = 0, last_winner, this_index;
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

    if(mr_miner_set_handlers(mask) == EXIT_FAILURE)
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
    if (mr_shm_init(&s_block, &s_net_data, &this_index) == EXIT_FAILURE)
    {
        free(mine_struct);
        free(workers);
        exit(EXIT_FAILURE);
    }
    sem_post(mutex);

    //BUCLE DE RONDAS DE MINADO

    while(n_rounds-- && !err){
        last_winner = (this_pid == s_net_data->last_winner);

        if(last_winner){
            //Preparar bloque y contar numero de mineros
            mr_shm_set_new_round(s_block, s_net_data);
            while(sem_wait(mutex) == -1);
            mr_shm_quorum(s_net_data);
            sem_post(mutex);
        }
        else
        {
            sigsuspend(&mask_sigusr1);
        }        

        //LANZAR TRABAJADORES
        err = mr_workers_launch(workers, mine_struct, n_workers,s_block->target);
        if(err) break;

        //Esperar a sigsuspend(SIGUSR2)
        sigsuspend(&mask_sigusr2);
        
        if(winner){
            // cargar_solucio
            mr_shm_set_solution(s_block, proof_solution);

            // avisar_mineros(); (mandar sigUsr2)

            //Matar a sus trabajdores
            mr_workers_cancel(workers, n_workers);

            // semáforo que dice que ha terminado la votación

            s_block->is_valid = 1;
            s_net_data->last_winner = this_pid;
            (s_block->wallets[this_index]) ++;
        }

        // else{
        //     res = comprobar_sol();
        //     votar(res) 
        //     si eres el último, up del terminado de votación
        //          
        //     Esperar a que se haya votado
        // }

        
        //Añadir bloque correcto a la cadena de cada minero
        last_block = mr_shm_block_copy(s_block, last_block);

        if(last_block == NULL)
            err = 1;

        if(winner){
            winner = 0;
            last_winner = 1; //CAMBIAR SEGUN VOTOS !!!!!
        }
    }
    
    print_blocks(last_block, 1);
    mr_blocks_free(last_block);


    shm_unlink(SHM_NAME_BLOCK);
    shm_unlink(SHM_NAME_NET);


    exit(EXIT_SUCCESS);
}