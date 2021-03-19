#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>

/**
 * @file conc_cycle.c
 * @authors
 * 
 * @brief
 * 
 * 
 */

#define SEM_NAME "/sem_cycles"
#define SECS 10

static volatile sig_atomic_t got_sigint = 0;
static volatile sig_atomic_t got_siguser = 0;
static volatile sig_atomic_t got_sigterm = 0;

void manejador(int sig){
    if(sig==SIGTERM){
        got_sigterm = 1;
    }else if(sig==SIGUSR1){
        got_siguser = 1;
    }else{/*also for SIGALRM*/
        got_sigint =1;
    }
}

int main(int argc, char *argv[]) {
    int NUM_PROC, i, term = 0;
    pid_t pid=0, this_pid, p1 = getpid(); /*pid del proceso 1*/
    struct sigaction act;
    sigset_t set, oset, setblock;
    sem_t *sem = NULL;

    if (alarm(SECS)){
        fprintf(stderr, "Existe una alarma previa establecida\n");
    }

    if (argc != 2) {
        fprintf(stderr, "Uso: %s <NUM_PROC>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    NUM_PROC = atoi(argv[1]);

    if ((sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED) {
		perror("sem_open");
		exit(EXIT_FAILURE);
	}
    sem_unlink(SEM_NAME);

    sigfillset(&setblock);
    sigprocmask(SIG_BLOCK,&setblock,NULL);
    
    sigemptyset(&set);
    sigemptyset(&(act.sa_mask));
    act.sa_handler = manejador;
    act.sa_flags = 0;

    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    sigdelset(&setblock, SIGUSR1);

    for (i = 0; i < NUM_PROC-1 && pid == 0; i++){
        if((pid=fork())<0){
            perror("fork");
            exit(EXIT_FAILURE);    
        }else if (pid == 0){
            if(i==0){/*el primer hijo*/
                sigdelset(&setblock, SIGTERM);
                /*sigaddset(&set, SIGALRM);
                sigaddset(&set, SIGINT);
                sigprocmask(SIG_BLOCK, &set, &oset);*/
                
                if (sigaction(SIGTERM, &act, NULL) < 0) {
                    perror("sigaction");
                    exit(EXIT_FAILURE);
                }
            }
        }else if (i==0){
            /*Proceso orginal*/
            sigdelset(&setblock, SIGINT);
            sigdelset(&setblock, SIGALRM);
            if (sigaction(SIGINT, &act, NULL) < 0) {
                perror("sigaction");
                exit(EXIT_FAILURE);
            }
            if (sigaction(SIGALRM, &act, NULL) < 0) {
                perror("sigaction");
                exit(EXIT_FAILURE);
            }
        }
    }

    /*ciclos*/

    /*Iniciamos los ciclos cuando todos los procesos se han creado */
    if(pid==0){
        if(kill(p1,SIGUSR1)==-1){
            perror("kill");
            exit(EXIT_FAILURE);
        }
    }

    pid = (!pid)?p1:pid;/*cada proceso manda a su hijo y el último al padre*/
    this_pid = getpid();

    for (i=0;!term;i++){
        
        sigsuspend(&setblock);/***bloquea todas menos las que espera***/
      
        if (got_siguser){
            got_siguser=0;

            sem_wait(sem);
            if(kill(pid,SIGUSR1)==-1){
                perror("kill");
                exit(EXIT_FAILURE);
            }
            printf("Ciclo: %i (PID=%d) \n", i, this_pid);
            sem_post(sem);
        }
        if (got_sigint){
            got_sigint = 0;
            if(kill(pid,SIGTERM)==-1){
                perror("kill");
                exit(EXIT_FAILURE);
            }
            term=1;            
        }
        if(got_sigterm){
            got_sigterm = 0;
            if(pid!=p1){
                if(kill(pid,SIGTERM)==-1){
                    perror("kill");
                    exit(EXIT_FAILURE);
                }
            }
            term=1;
        }
    }

    sem_close(sem);
    wait(NULL);
    exit(EXIT_SUCCESS);
}

/*
Despues de la llamada a la función fork, el proceso hijo:
    Hereda la máscara de señales bloqueadas.
    Tiene vacía la lista de señales pendientes.
    Hereda las rutinas de manejo.
    No hereda las alarmas.
*/