#include "miner.h"
#include "mr_util.h"
#include "mr_monitor.h"
#include <stdint.h>

static volatile sig_atomic_t got_sigint = 0;
volatile sig_atomic_t got_sigalrm = 0;

void handler_sigint(int sig)
{
    got_sigint = 1;
}

void handler_sigalrm(int sig)
{
    got_sigalrm = 1;
}

/**
 * @brief hijo
 * 
 * @param fd 
 */
void mr_monitor_printer(int fd[2])
{
    Block *last_block = NULL;
    Block block;
    char filename[1024];
    int file;
    struct sigaction act;
    int ret, err = 0, n_wallets = 0;
    sigset_t mask;
    close(fd[1]);

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    sigemptyset(&(act.sa_mask));
    act.sa_handler = handler_sigalrm;
    act.sa_flags = 0;

    if (sigaction(SIGALRM, &act, NULL) < 0)
    {
        perror("sigaction SIGALRM");
        close(fd[0]);
        exit(EXIT_FAILURE);
    }

    sprintf(filename, "monitor%jd.log", (intmax_t)getpid());

    if ((file = open(filename, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1)
    {
        perror("open");
        close(fd[0]);
        exit(EXIT_FAILURE);
    }

    /*poner la primera alarma*/
    err = mr_printer_handle_sigalrm(last_block, n_wallets, file);

    while (!err && (ret = mr_fd_read_block(&block, fd, last_block, n_wallets, file, &err) > 0))
    {
        n_wallets = block.is_valid;
        block.is_valid = 1;
        last_block = mr_shm_block_copy(&block, last_block);

        if (last_block == NULL)
            err = 1;
        if (!err && got_sigalrm)
        {
            err = mr_printer_handle_sigalrm(last_block, n_wallets, file);
        }
    }

    close(fd[0]);
    if (!err)
        print_blocks_file(last_block, n_wallets, file);
    mr_blocks_free(last_block);
    close(file);

    exit((err || (ret < 0)) ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    int fd[2], err = 0, i;
    pid_t child;
    mqd_t queue;
    NetData *s_net_data;
    sem_t *mutex;
    struct sigaction act;
    Monitor_blocks blocks;
    Block block;
    blocks.last_block = 0;

    if (pipe(fd) == -1)
    { /*apertura de los ficheros de la tubería*/
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    child = fork();
    if (child < 0)
    {
        perror("fork");
        close(fd[0]);
        close(fd[1]);
        exit(EXIT_FAILURE);
    }
    else if (child == 0)
    {
        mr_monitor_printer(fd); // el hijo termina por su cuenta
    }
    close(fd[0]);

    sigemptyset(&(act.sa_mask));
    act.sa_handler = handler_sigint;
    act.sa_flags = 0;

    if (sigaction(SIGINT, &act, NULL) < 0)
    {
        perror("sigaction SIGALRM");
        close(fd[1]);
        exit(EXIT_FAILURE);
    }

    if ((mutex = sem_open(SEM_MUTEX_NAME, O_CREAT, S_IRUSR | S_IWUSR, 1)) == SEM_FAILED)
    {
        perror("sem_open");
        close(fd[1]);
        exit(EXIT_FAILURE);
    }

    while (sem_wait(mutex) == -1);
    if (mr_shm_init_monitor(&s_net_data) == EXIT_FAILURE)
    {
        close(fd[1]);
        sem_post(mutex);/*en el caso de que ya haya monitor, para que la red no se detenga*/
        sem_close(mutex);
        exit(EXIT_FAILURE);
    }
    sem_post(mutex);

    queue = mr_monitor_mq_open(MQ_MONITOR_NAME, O_CREAT | O_RDONLY);
    if (queue == (mqd_t)-1)
    {
        close(fd[1]);
        sem_close(mutex);
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < MAX_BLOCKS; i++)
        blocks.buffer[i].id = -1;

    while (!err && !got_sigint)
    {
        err = mr_mq_receive(&block, queue);

        if (!err && !mr_monitor_block_is_repeated(&block, &blocks, &err))
        { //ver que es correcto y no repetido
            block.is_valid = s_net_data->last_miner + 1;
            err = mr_fd_write_block(&block, fd);
        }
    }

    close(fd[1]);

    mr_monitor_close_net_mutex(mutex, s_net_data);

    wait(NULL);
    exit(EXIT_SUCCESS);
}
