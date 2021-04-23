#include "miner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mqueue.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <errno.h>


typedef struct Mine_struct{
    long int target;
    long int begin;
    long int end;
}Mine_struct;


int proof_solved=0;
long int proof_solution;

void *mine(void *d){
    Mine_struct *data = (Mine_struct*)d;
    long int i;
    for(i=0;i<data->end && !proof_solved ;i++){
        if(data->target=simple_hash(i)){
            proof_solution=i;
        }
    }

    return NULL;    
}

int mr_init_shm(Block** b, NetData **d){
    int fd_b, fd_d, exists = 0, err = 0;
    struct stream_t *stream_shm;
        
    /*creación del segmento de memoria compartida*/
    if ((fd_b = shm_open(SHM_NAME_BLOCK, O_RDWR | O_CREAT | O_EXCL,
                           S_IRUSR | S_IWUSR)) == -1){
        if(errno == EEXIST){
            exists = 1;
            if((fd_b = shm_open(SHM_NAME_BLOCK, O_RDWR, 0)) == -1)
                err = 1;
        }
        else
            err = 1;
            
        if(err){
            perror("shm_open");
            return EXIT_FAILURE;
        }
    }

    

    /*mapear el segmento de memoria*/
    stream_shm = mmap(NULL, sizeof(struct stream_t),
                      PROT_WRITE | PROT_READ, MAP_SHARED, fd_shm, 0);
    close(fd_shm);
    if (stream_shm == MAP_FAILED)
    {
        perror("mmap");
        return NULL;
    }
}


/*
 Liberar:
    workers
 */

int main(int argc, char *argv[]){
    int n_workers, n_rounds, err = 0,i;
    pthread_t *workers = NULL;
    Block *s_block;
    NetData *s_net_data;
    Mine_struct *mine_struct=NULL;

    if (argc != 2){
        fprintf(stderr, "Uso: %s <Numero_de_trabajadores> <Número_de_rondas>", argv[0]);
        exit(EXIT_SUCCESS);
    }

    n_workers=atoi(argv[1]);
    n_rounds=atoi(argv[2]);
    
    if(!(workers=(pthread_t *)calloc(n_workers,sizeof(pthread_t)))){
        exit(EXIT_FAILURE);
    }

    if(!(mine_struct=(Mine_struct *)calloc(n_workers,sizeof(Mine_struct)))){
        exit(EXIT_FAILURE);
    }

    if(mr_init_shm(&s_block, &s_net_data) == EXIT_FAILURE){
        free(workers);
        eixt(EXIT_FAILURE);
    }

    //ACCEDER A LA MEMORIA COMPARTIDA





    //BUCLE DE RONDAS

    while(n_rounds-- && !err){

        /**
         * if(ultimo_ganador){
         *      Inicializar el bloque
         *      ver cuantos mineros hay (SIGUSR1) --> actualizamos N_MINERS
         *      for(ups(),N_MINERS)
         * }
         */


        //LANZAR MINEROS
        for(i=0;i<n_workers;i++){
            
            //mine_struct[i].target=
            //mine_struct[i].begin=
            //mine_struct[i].end=

            if ((err = pthread_create(&workers[i], NULL, mine, (void *)(&mine_struct[i]))) != 0){
                fprintf(stderr, "pthread_create: %s\n", strerror(err));
                exit(EXIT_FAILURE);
            }
        }
     
       
        

    }
    exit(EXIT_SUCCESS);
}