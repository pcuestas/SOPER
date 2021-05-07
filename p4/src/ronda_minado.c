/***********************************************/
/* Programa: ronda_minado       Fecha:1/5/2021 */
/* Autores: Celia Jimenez y Pablo Fernandez    */
/*                                             */
/* Programa que lanza las rondas de minado     */				                   
/* Input: nrondas : numero de rondas           */
/* Output: EXIT_SUCCESS:OK, EXIT_FAILURE: ERR  */
/***********************************************/
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <time.h>
#include <signal.h>
#include <mqueue.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include "miner.h"

#define SHM_NAME_NET "/netdata"

/*Programa en el que llevamos a cabo <nrondas> de minado*/
int main(int argc, char *argv[]) {
	int i=0, j=0, nrondas=0, creado=0;
    NetData *net = NULL;
        
    /*COMPROBACION DE ARGUMENTOS*/
    if(argc != 2){
        printf("input should be:\n\t./ronda_minado <nrondas>\n");
        exit(EXIT_FAILURE);
    }

    if((nrondas = atoi(argv[1])) < 1){
        printf("nrondas should be > 0\n");
        exit(EXIT_FAILURE);
    }

    /*abrimos red*/
    open_net(&net, &creado);
    if(creado == 1){
        printf("no hay mineros creados\n");
        munmap(net, sizeof(net));
        shm_unlink(SHM_NAME_NET);
        exit(EXIT_FAILURE);
    }

    printf("%d\n", net->monitor_pid);
    /*RONDAS DE MINADO*/
    for(i=0; i<nrondas; i++){
        sem_wait(&net->mutex);
        printf("celia\n");
        for(j=0; j<net->total_miners; j++){
            sem_post(&net->empezar_ronda);
        }
        sem_post(&net->mutex);
        sem_wait(&net->ronda_acabada);
    }

    munmap(net, sizeof(NetData));
    exit(EXIT_SUCCESS);
}
