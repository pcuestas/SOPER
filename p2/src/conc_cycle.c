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
#define SECS 10

static volatile sig_atomic_t got_sigusr1 = 0;
static volatile sig_atomic_t got_sigterm = 0;

/**
 * @brief manejador - para cada señal actualiza la 
 *  bandera correspondiente
 */
void manejador(int sig){
    if (sig == SIGUSR1){
        got_sigusr1 = 1;
    }
    else{ /*SIGINT, SIGALRM o SIGTERM*/
        got_sigterm = 1;
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
    if(kill(pid,sig)==-1){
        perror("kill");
        exit(EXIT_FAILURE);
    }
}

pid_t child(int j, int NUM_PROC){
    pid_t next;
    if(j < NUM_PROC){
        /*creación de el hijo*/
        if((next = fork())<0){
            perror("fork");
            exit(EXIT_FAILURE);    
        }else if (next == 0){
            return child(j+1, NUM_PROC);
        }else{
            return next;
        }
    }else{
        /*el último hijo no hace nada*/
        return 0;
    }
}

void signal_and_print(sem_t *sem, int cycle_number, pid_t next, pid_t this_pid){
    sem_wait(sem);
    kill_(next,SIGUSR1);
    printf("Ciclo: %d (PID=%jd)\n", cycle_number, (intmax_t)this_pid);
    sem_post(sem);
}

void cycles(int first_cycle, pid_t next, pid_t this_pid, pid_t first_proc, sem_t *sem, sigset_t *set){
    int i, term;
    for (i = first_cycle, term = 0; !term; i++){
        sigsuspend(set);/*bloquea todas las señales menos las que espera cada proceso*/
      
        if (got_sigusr1){
            got_sigusr1 = 0;

            signal_and_print(sem, i, next, this_pid);
        }
        else if(got_sigterm){
            got_sigterm = 0;

            if(next!=first_proc){
                kill_(next,SIGTERM);
            }
            term = 1;
        }
    }
}

int main(int argc, char *argv[]) {
    int NUM_PROC, first_cycle, term;
    pid_t pid = 0, this_pid, next, p1 = getpid(); /*pid del proceso 1*/
    struct sigaction act;
    sigset_t set;
    sem_t *sem = NULL;

    if (alarm(SECS)){
        fprintf(stderr, "Existe una alarma previa establecida\n");
    }

    if (argc != 2 || (NUM_PROC = atoi(argv[1]))<=1) {
        fprintf(stderr, "Uso:\t%s <NUM_PROC>\n\tCon NUM_PROC > 1\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if ((sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED) {
		perror("sem_open");
		exit(EXIT_FAILURE);
	}
    sem_unlink(SEM_NAME);

    /*bloquear todas las señales antes de llamar a sigaction*/
    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, NULL);
    sigdelset(&set, SIGUSR1); /*para poder capturarla en los ciclos*/
    sigdelset(&set, SIGTERM);

    set_act(&act);/*establecer act*/


    /*Establecer manejadores de las señales*/
    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGTERM, &act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGINT, &act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGALRM, &act, NULL) < 0){
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    /*creación de el hijo*/
    if((pid=fork())<0){
        perror("fork");
        exit(EXIT_FAILURE);    
    }else if (pid == 0){
        sigdelset(&set, SIGTERM);/* el primer hijo, el resto lo hereda */
        next = child(1, NUM_PROC);/* todos los hijos vuelven aquí */
        this_pid = getpid();
        first_cycle = 0;
    }else{
        /*padre */
        sigdelset(&set, SIGINT);
        sigdelset(&set, SIGALRM);

        /*primera señal al hijo*/
        this_pid = p1;
        pid = next;
        signal_and_print(sem, 0, next, this_pid);
        first_cycle = 1;
    }
    /*común para todos los procesos*/

    next = (!next) ? p1 : next;

    cycles(first_cycle, next, this_pid, p1, sem, &set);

    sem_close(sem);
    wait(NULL);
    exit(EXIT_SUCCESS);
}
