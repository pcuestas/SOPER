#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <stdint.h>

/**
 * @file conc_cycle.c
 * @authors Pablo Cuesta Sierra, Álvaro Zamanillo Sáez
 * 
 * @brief Programa descrito en el ejercicio 10 de la 
 *  práctica 2 (SOPER).
 * 
 */

#define SEM_NAME "/sem_cycles"
#define MAX_SECS 10

static volatile sig_atomic_t got_sigusr1 = 0;
static volatile sig_atomic_t got_end = 0;

/**
 * @brief manejador - para cada señal actualiza la 
 *  bandera correspondiente
 */
void manejador(int sig){
    if (sig == SIGUSR1){
        got_sigusr1 = 1;
    }
    else{ /*SIGINT, SIGALRM o SIGTERM*/
        got_end = 1;
    }
}

/**
 * @brief Configura la acción que se va a utilizar para 
 *  capturar las señales. Bloquea las señales que 
 *  pueden recibir los procesos durante los ciclos para
 *  evitar que se pierdan.
 * @param act puntero a la estructura de sigaction que
 *  se va a configurar.
 */
void set_act(struct sigaction *act){
    /*bloqueamos las señales dentro del manejador 
    para evitar perderlas*/
    sigemptyset(&(act->sa_mask));
    sigaddset(&(act->sa_mask), SIGALRM);
    sigaddset(&(act->sa_mask), SIGINT);
    sigaddset(&(act->sa_mask), SIGTERM);
    sigaddset(&(act->sa_mask), SIGUSR1);

    act->sa_handler = manejador;
    act->sa_flags = 0;
}

/**
 * @brief Wrapper de kill con control de error. 
 * En caso de error, termina el proceso e imprime 
 * el error obtenido.
 * @param pid pid igual que la función kill(2)
 * @param sig sig igual que la función kill(2)
 */
void kill_(pid_t pid, int sig){
    if (kill(pid, sig) == -1){
        perror("kill");
        exit(EXIT_FAILURE);
    }
}

/**
 * @brief Wrapper de sigaction con control de error. 
 * En caso de error, termina el proceso e imprime 
 * el error obtenido.
 * @param signum igual que la función sigaction(2)
 * @param act igual que la función sigaction(2)
 * @param oldact igual que la función sigaction(2)
 */
void sigaction_(int sig, const struct sigaction *act, struct sigaction *oldact){
    if (sigaction(sig, act, oldact) < 0){
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

void signal_next_process_and_print(sem_t *sem, int cycle_number, pid_t next, pid_t this_pid){
    while(sem_wait(sem) != 0); /*para evitar que devuelva y no sea por el sem*/
    
    kill_(next, SIGUSR1);
    printf("Ciclo: %d (PID=%jd)\n", cycle_number, (intmax_t)this_pid);

    sem_post(sem);    
}

void cycles(pid_t next, pid_t first_proc, sigset_t *susp_mask, sem_t *sem){

    pid_t this_pid = getpid();

    /* P1 ya ha hecho el ciclo 0 */
    int cycle_num = (this_pid == first_proc) ? 1 : 0;

    /*cada proceso manda señales a su hijo y el último a P1*/
    next = (!next) ? first_proc : next; 

    while(!got_end){
        /*bloquea todas las señales menos las que espera el proceso*/
        sigsuspend(susp_mask);

        if (got_sigusr1){
            got_sigusr1 = 0;

            signal_next_process_and_print(sem, cycle_num, next, this_pid);
            cycle_num++;
        }
        else if (got_end){
            /*Todos los procesos salvo el último tienen que mandar SIGTERM al siguiente*/
            if (next != first_proc){
                kill_(next, SIGTERM);
            }
        }
    }
}

int main(int argc, char *argv[]){
    int NUM_PROC, i, term;
    pid_t pid = 0, this_pid, first_proc = getpid(); /*pid del proceso 1*/
    struct sigaction act;
    sigset_t set;
    sem_t *sem = NULL;

    /*Control de argumentos de entrada*/
    if (argc != 2 || (NUM_PROC = atoi(argv[1])) <= 1){
        fprintf(stderr, "Uso:\t%s <NUM_PROC>\n\tCon NUM_PROC > 1\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /*Establecer alarma*/
    if (alarm(MAX_SECS)){
        fprintf(stderr, "Existe una alarma previa establecida\n");
    }


    if ((sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED){
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
    sem_unlink(SEM_NAME);


    /*bloquear todas las señales antes de llamar a sigaction*/
    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, NULL);
    sigdelset(&set, SIGUSR1); /*para poder capturarla en los ciclos*/
    set_act(&act); /*establecer act*/

    /*Señal común para todos los procesos*/
    sigaction_(SIGUSR1, &act, NULL);

    /*Creación de P2 en cascada*/
    if ((pid = fork()) < 0){
        perror("Fork ");
        exit(EXIT_FAILURE);
    }
    else if (pid > 0){ /*P1*/
        sigdelset(&set, SIGINT);
        sigdelset(&set, SIGALRM);
        sigaction_(SIGINT, &act, NULL);
        sigaction_(SIGALRM, &act, NULL);

        /*Inicia los ciclos mandando SIGUSR1 a su hijo e imprimiendo en stdout*/
        signal_next_process_and_print(sem, 0, pid, first_proc);
    }
    else{   
        /*Señales comunes para los procesos a partir de P2*/
        sigdelset(&set, SIGTERM);
        sigaction_(SIGTERM, &act, NULL);

        /*Cada proceso crea un solo hijo hasta que hay NUM_PROC procesos en total: creación de procesos en cascada*/
        for (i = 2; i < NUM_PROC && pid == 0; i++){
            if ((pid = fork()) < 0){
                perror("fork");
                exit(EXIT_FAILURE);
            }
        }
    }
    /*Código común para todos los procesos*/
    
    /*Realizar los ciclos*/
    cycles(pid, first_proc, &set, sem);
    
    /*Liberar y esperar a su hijo, si lo tiene*/
    sem_close(sem);
    wait(NULL);
    exit(EXIT_SUCCESS);
}
