/***********************************************/
/* Programa: lanzar_mineros    Fecha:1/5/2021  */
/* Autores: Celia Jimenez y Pablo Fernandez    */
/*                                             */
/* Programa que lanza los mineros              */				                   
/* Input: nmineros : numero de mineros         */
/*        ntrabajadores : numero de hilos      */
/*        nrondas : numero de rondas           */
/* Output: EXIT_SUCCESS:OK, EXIT_FAILURE: ERR  */
/***********************************************/
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_WORKERS 10
#define MAX_MINERS 200
#define MINER "./mr_miner"

int main(int argc, char *argv[]) {
	int i=0;
	pid_t pid;
    int ntrabajadores, nrondas, nmineros;
    
    /*COMPROBACION DE ARGUMENTOS*/
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <nmineros> <ntrabajadores> <nrondas> \n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /*numero de mineros que lanza el programa*/
    nmineros = atoi(argv[1]);
    if(nmineros<1 || nmineros>MAX_MINERS){
        fprintf(stderr, "nmineros tiene que ser mayor que 0 y menor que 200\n");
        exit(EXIT_FAILURE);    	
    }

	/*numero de trabajadores que tiene que lanzar el minero*/
    ntrabajadores = atoi(argv[2]);
    
    /*numero de rondas de minado que tiene que realizar el minero*/
    nrondas = atoi(argv[3]);

    /*lanzamos todos los mineros*/
    for(i=0; i<nmineros; i++){
        pid=fork(); 
        if(pid < 0){
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if(pid == 0){
            if(execl(MINER, MINER, argv[2], argv[3], (char*)NULL)==-1){
                perror("execl");
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS);
        }
    }
    
    for(i=0; i<nmineros; i++){
        wait(NULL);
    }
    printf("TODO BIEN\n");
    exit(EXIT_SUCCESS);
}
