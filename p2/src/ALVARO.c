#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>

#define SECS 10
#define SEM_NAME "/my_sem"

static volatile sig_atomic_t got_sigint = 0;
static volatile sig_atomic_t got_siguser = 0;
static volatile sig_atomic_t got_sigterm = 0;

void manejador(int sig)
{
    if (sig == SIGINT)
    {
        got_sigint = 1;
    }
    else if (sig == SIGUSR1)
    {
        got_siguser = 1;
    }
    else
    {
        got_sigterm = 1;
    }
}

int main(int argc, char *argv[])
{

    int i, nPROC, ciclo = 0;
    pid_t pid, p1, env;
    sem_t *sem = NULL;

    struct sigaction act;
    sigset_t set, oset;

    if (argc != 2)
    {
        fprintf(stderr, "One argument needed.\n./conc_cycle <NUM_PROC>\n");
        return 0;
    }

    if ((sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED)
    {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    sem_unlink(SEM_NAME);

    if (alarm(SECS))
    {
        fprintf(stderr, "Existe una alarma previa establecida\n");
    }

    act.sa_handler = manejador;
    sigemptyset(&(act.sa_mask));
    act.sa_flags = 0;

    sigemptyset(&set);

    nPROC = atoi(argv[1]);
    p1 = getpid();

    /*Todos los procesos van a manejar la señal SIGUSR1*/
    if (sigaction(SIGUSR1, &act, NULL) < 0)
    {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < nPROC - 1; i++)
    {
        pid = fork();

        if (pid < 0)
        {
            perror("Error during fork: ");
            exit(EXIT_FAILURE);
        }
        else if (pid == 0)
        {
            /*Hijo*/

            /*Todos los procesos salvo el primero van a bloquear la señal SIGINT*/
            if (i == 0)
            {
                sigaddset(&set, SIGINT);
                sigaddset(&set, SIGALRM);

                if (sigaction(SIGTERM, &act, NULL) < 0)
                {
                    perror("sigaction");
                    exit(EXIT_FAILURE);
                }
            }
        }
        else
        {

            /*Proceso original*/

            if (i == 0)
            {
                if (sigaction(SIGINT, &act, NULL) < 0)
                {
                    perror("sigaction");
                    exit(EXIT_FAILURE);
                }

                if (sigaction(SIGALRM, &act, NULL) < 0)
                {
                    perror("sigaction");
                    exit(EXIT_FAILURE);
                }
            }

            break;
        }
    }

    sigprocmask(SIG_BLOCK, &set, &oset);

    i = 0;

    /*Iniciamos los ciclos*/

    if (pid == 0)
    {
        if (kill(p1, SIGUSR1) != 0)
        {
            perror("Inicio ");
            exit(EXIT_FAILURE);
        }
    }

    /*CICLOS*/

    while (1)
    {
        sigsuspend(&set);
        {
            got_sigint = 0;
            if (kill(pid, SIGUSR1) != 0)
            {
                perror("");
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS);
        }
        else if (got_siguser == 1)
        {
            got_siguser = 0;
            /*while(sem_trywait(sem)==-1)*/
            /*while(sem_wait(sem)==-1)*/

            env = (pid == 0) ? p1 : pid;
            if (kill(env, SIGUSR1) != 0)
            {
                perror("Error ciclo");
                exit(EXIT_FAILURE);
            }

            printf("Ciclo número %i (PID=%d) \n", i, getpid());
            i++;

            sem_post(sem);
        }
        else
        {
            got_sigterm = 0;
            env = (pid == 0) ? p1 : pid;
            sem_close(sem);
            if (env == p1)
                exit(EXIT_SUCCESS);

            if (kill(env, SIGUSR1) != 0)
            {
                perror("");
                exit(EXIT_FAILURE);
            }

            wait(NULL); /*Esperar al proceso hijo*/
            exit(EXIT_SUCCESS);
        }
    }

    return 0;
}