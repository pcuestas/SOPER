#include "miner.h"
#include "mr_util.h"
#include "mr_monitor.h"
#include <stdint.h>

volatile sig_atomic_t got_sigint  = 0; /*flag para SIGINT */
volatile sig_atomic_t got_sigalrm = 0; /*flag para SIGALRM*/

/**
 * @brief Función principal del hijo del monitor
 * 
 * @param fd tubería abierta
 */
void mrtp_printer_main(int fd[2])
{
    Block *last_block = NULL;
    Block block;
    char filename[1024];
    int file;
    struct sigaction act;
    int ret, err = 0, n_wallets = 0;
    sigset_t mask;
    close(fd[1]);

    /*bloquear SIGINT para que solo la reciba el padre*/
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
    err = mrp_handle_sigalrm(last_block, n_wallets, file);

    while (!err && (ret = mrp_fd_read_block(&block, fd, last_block, n_wallets, file, &err) > 0))
    {
        n_wallets = block.is_valid; /*recibimos el número de wallets por este campo no necesario*/
        block.is_valid = 1;
        last_block = mr_block_append(&block, last_block);

        if (last_block == NULL)
            err = 1;
        if (!err && got_sigalrm)
            err = mrp_handle_sigalrm(last_block, n_wallets, file);
    }

    close(fd[0]);
    if (!err)
        mr_blocks_print_to_file(last_block, n_wallets, file);
    mr_blocks_free(last_block);
    close(file);

    exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    int fd[2], err = 0, i;
    pid_t child;
    mqd_t queue;
    NetData *s_net_data;
    sem_t *mutex;
    struct sigaction act;
    MonitorBlocks blocks;
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
        mrtp_printer_main(fd); /* el hijo termina por su cuenta*/
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
    if (mrt_shm_init(&s_net_data) == EXIT_FAILURE)
    {
        close(fd[1]);
        sem_post(mutex);/*en el caso de que ya haya monitor, para que la red no se detenga*/
        sem_close(mutex);
        exit(EXIT_FAILURE);
    }
    sem_post(mutex);

    queue = mr_mq_open(MQ_MONITOR_NAME, O_CREAT | O_RDONLY);
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
        err = mrt_mq_receive(&block, queue);

        if (!err && !mrt_block_is_repeated(&block, &blocks, &err))
        { /*si no repetido, escibirlo en la tubería*/
            block.is_valid = s_net_data->last_miner + 1;
            err = mrt_fd_write_block(&block, fd);
        }
    }

    close(fd[1]);

    mrt_close_net_mutex(mutex, s_net_data);

    wait(NULL); /*esperar al hijo*/
    exit(EXIT_SUCCESS);
}
