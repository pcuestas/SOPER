#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * @file conc_cycle.c
 * @authors
 * 
 * @brief
 * 
 * 
 */

int main(int argc, char *argv[]) {
    int NUM_PROC, i;
    pid_t pid, ppid = getpid(); /*pid del padre*/

    if (argc != 2) {
        fprintf(stderr, "Uso: %s <NUM_PROC>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    NUM_PROC = atoi(argv[1]);

    for (i = 0; i < NUM_PROC-1 && (pid=fork())!=0; i++);

    if(pid<0){
        perror("fork");
        exit(EXIT_FAILURE);
    }

    exit(EXIT_SUCCESS);
}

/*
Despues de la llamada a la función fork, el proceso hijo:
    Hereda la máscara de señales bloqueadas.
    Tiene vacía la lista de señales pendientes.
    Hereda las rutinas de manejo.
    No hereda las alarmas.
*/