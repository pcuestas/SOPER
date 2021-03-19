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

static volatile sig_atomic_t got_sigint = 0;
static volatile sig_atomic_t got_sigusr1 = 0;
static volatile sig_atomic_t got_sigterm = 0;

/**
 * @brief manejador - para cada señal actualiza la 
 *  bandera correspondiente
 */
void manejador(int sig){
    if (sig == SIGTERM){
        got_sigterm = 1;
    }
    else if (sig == SIGUSR1){
        got_sigusr1 = 1;
    }
    else{ /*SIGINT o SIGALRM*/
        got_sigint = 1;
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

/**
 * @brief Establece el set de los hijos 
 * (para sigsuspend) y la rutina de manejo 
 * que solo tienen los hijos
 */
void set_and_act_child(sigset_t *set, struct sigaction *act){
    sigdelset(set, SIGTERM);
                
    if (sigaction(SIGTERM, act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}
/**
 * @brief Establece el set del padre (para sigsuspend) y 
 * las rutinas de manejo de señales exclusivas del padre
 */
void set_and_act_parent(sigset_t *set, struct sigaction *act){
    sigdelset(set, SIGINT);
    sigdelset(set, SIGALRM);

    if (sigaction(SIGINT, act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGALRM, act, NULL) < 0){
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char *argv[]) {
    int NUM_PROC, i, term;
    pid_t pid = 0, this_pid, p1 = getpid(); /*pid del proceso 1*/
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
    
    set_act(&act);/*establecer act*/

    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    /*creación de los hijos*/
    for (i = 0; i < NUM_PROC-1 && pid == 0; i++){
        if((pid=fork())<0){
            perror("fork");
            exit(EXIT_FAILURE);    
        }else if (i==0){
            if(pid == 0){/*el primer hijo solo-el resto lo hereda*/
                set_and_act_child(&set, &act);
            }else{/*proceso padre original*/
                set_and_act_parent(&set, &act);
            }
        }
    }

    /*ciclos*/
    
    /* Iniciamos los ciclos cuando todos los procesos se 
     * han creado -- el último proceso creado manda señal al padre*/
    if(pid==0){
        kill_(p1,SIGUSR1);
    }

    this_pid = getpid();
    pid = (!pid) ? p1 : pid; /*cada proceso manda señales a su hijo y el último al padre*/

    for (i = 0, term = 0; !term; i++){
        sigsuspend(&set);/*bloquea todas las señales menos las que espera cada proceso*/
      
        if (got_sigint){
            got_sigint = 0;

            kill_(pid,SIGTERM);
            term=1;            
        }
        else if(got_sigterm){
            got_sigterm = 0;

            if(pid!=p1){
                kill_(pid,SIGTERM);
            }
            term=1;
        }
        else if (got_sigusr1){
            got_sigusr1=0;

            sem_wait(sem);
            kill_(pid,SIGUSR1);
            printf("Ciclo: %d (PID=%jd)\n", i, (intmax_t)this_pid);
            sem_post(sem);
        }
    }

    sem_close(sem);
    wait(NULL);
    exit(EXIT_SUCCESS);
}
