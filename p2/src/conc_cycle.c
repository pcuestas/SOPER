/**
 * @file conc_cycle.c
 * @author Pablo Cuesta Sierra, Álvaro Zamanillo Sáez
 * 
 * @brief Programa descrito en el ejercicio 10 de la 
 *  práctica 2 (SOPER).
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdint.h>

#define SEM_NAME "/sem_conc_cycle"
#define MAX_SEC 10

static volatile sig_atomic_t got_sigusr1 = 0;
static volatile sig_atomic_t got_end = 0;

/**
 * @brief Para cada señal actualiza la 
 *  bandera correspondiente:
 *     got_sigusr1 para SIGUSR1;
 *     got_end     en el caso de SIGINT, 
 *                 SIGALRM o SIGTERM.
 */
void handler(int sig){
    if (sig == SIGUSR1){
        got_sigusr1 = 1;
    }
    else{ /*SIGINT, SIGALRM o SIGTERM*/
        got_end = 1;
    }
}

/**
 * @brief Manda la señal SIGUSR1 al proceso siguiente 
 *  e imprime la cadena correspondiente al número de ciclo.
 *  Protege esta acción con el semáforo binario sem.
 *
 * @param sem semáforo binario que protege el envío de la señal
 *  y la impresión en stdout, para asegurar que ningún proceso 
 *  pueda imprimir antes que el anterior. 
 * @param cycle_number número del ciclo
 * @param next_proc siguiente proceso (al que se manda SIGUSR1)
 * @param this_pid pid del proceso que se está ejecutando (el que
 *  envía la señal)
 */
void signal_next_process_and_print(sem_t *sem, int cycle_number, pid_t next_proc, pid_t this_pid){
    while(sem_wait(sem) != 0); /*para evitar que devuelva y no sea por el sem*/
    
    if (kill(next_proc, SIGUSR1) == -1){
        perror("kill");
        sem_close(sem);
        exit(EXIT_FAILURE);
    }
    printf("Ciclo: %d (PID=%jd)\n", cycle_number, (intmax_t)this_pid);

    sem_post(sem);    
}

/**
 * @brief ejecuta el bucle con los ciclos (bloquean 
 * el proceso hasta que recibe señal, y trata la 
 * señal que se ha recibido). Termina cuando se recibe
 * la señal que indica la terminación de los ciclos, tras 
 * mandar al proceso siguiente SIGTERM. 
 * 
 * @param next_proc pid del proceso hijo del que 
 * llama a esta función, o 0 si es el último proceso
 * @param first_proc pid del proceso 1
 * @param old_mask la máscara con la que se llama a 
 * sigsuspend (permite la llegada de las señales que
 * puede recibir el proceso en cada ciclo)
 * @param sem semáforo que se utiliza para asegurar 
 * el orden de impresión por stdout
 */
void cycles(pid_t next_proc, pid_t first_proc, sigset_t *old_mask, sem_t *sem){

    pid_t this_pid = getpid();

    /* P1 ya ha hecho el ciclo 0 */
    int cycle_num = (this_pid == first_proc) ? 1 : 0;

    /*cada proceso manda señales a su hijo y el último a P1*/
    next_proc = (!next_proc) ? first_proc : next_proc;

    while(!got_end){
        /*Esperamos que se reciba una de las señales previamente bloqueadas*/
        sigsuspend(old_mask);

        if (got_sigusr1){
            got_sigusr1 = 0;

            signal_next_process_and_print(sem, cycle_num, next_proc, this_pid);
            cycle_num++;
        }
    }
    
    /*Todos los procesos salvo el último tienen que mandar SIGTERM al siguiente*/
    if ((next_proc != first_proc) && (kill(next_proc, SIGTERM) == -1)){
        perror("kill");
        sem_close(sem);
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]){
    int NUM_PROC, i;
    pid_t next_proc;
    pid_t first_proc = getpid(); /*pid del proceso 1*/
    
    struct sigaction act;
    sigset_t mask, old_mask;
    sem_t *sem = NULL;

    /*Control de argumentos de entrada*/
    if ((argc != 2) || ((NUM_PROC = atoi(argv[1])) <= 1)){
        fprintf(stderr, "Uso:\t%s <NUM_PROC>\n\tCon NUM_PROC > 1\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /*Establecer alarma*/
    if (alarm(MAX_SEC)){
        fprintf(stderr, "Existe una alarma previa establecida\n");
    }

    if ((sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED){
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    sem_unlink(SEM_NAME);


    /*Bloquear las señales antes de llamar a sigaction (para evitar perderlas antes de que se llame a sigsuspend)*/
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGALRM);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, &old_mask); /*guardamos old_mask para luego usarla en sigsuspend*/


    /*Establecer la estructura act*/
    act.sa_mask = mask; /*para que dentro del manejador se bloqueen las señales*/
    act.sa_handler = handler;
    act.sa_flags = 0;


    /*Manejadores de las señales que se pueden recibir*/
    if (sigaction(SIGUSR1, &act, NULL) < 0){
        perror("sigaction SIGUSR1");
        sem_close(sem);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGINT, &act, NULL) < 0){
        perror("sigaction SIGINT");
        sem_close(sem);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGALRM, &act, NULL) < 0){
        perror("sigaction SIGALRM");
        sem_close(sem);
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &act, NULL) < 0){
        perror("sigaction SIGTERM");
        sem_close(sem);
        exit(EXIT_FAILURE);
    }



    /*Creación de P2*/
    if ((next_proc = fork()) < 0){
        perror("fork");
        sem_close(sem);
        exit(EXIT_FAILURE);
    }
    else if (next_proc > 0){ /*P1*/

        /*Inicia los ciclos mandando SIGUSR1 a su hijo e imprimiendo en stdout*/
        signal_next_process_and_print(sem, 0, next_proc, first_proc);
    }
    else{   
        /*Evitar que los procesos que no son el 1 reciban SIGINT durante sigsuspend*/
        sigaddset(&old_mask, SIGINT);

        /*Cada proceso crea un solo hijo hasta que hay NUM_PROC procesos en total: creación de procesos en cascada*/
        for (i = 2; (i < NUM_PROC) && (next_proc == 0); i++){
            if ((next_proc = fork()) < 0){
                perror("fork");
                sem_close(sem);
                exit(EXIT_FAILURE);
            }
        }
    }



    /*Código común para todos los procesos*/
    
    /*Realizar los ciclos*/
    cycles(next_proc, first_proc, &old_mask, sem);
    
    /*Liberar y esperar a su hijo (si lo tiene)*/
    sem_close(sem);
    wait(NULL);
    
    exit(EXIT_SUCCESS);
}
