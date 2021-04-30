#include "mr_monitor.h"
#include "mr_util.h"


extern volatile sig_atomic_t got_sigalrm;

int mr_shm_init_monitor(NetData **d)
{
    int created;

    created = mr_shm_map(SHM_NAME_NET, (void **)d, sizeof(NetData));

    if (created == MR_SHM_FAILED)
    {
        return EXIT_FAILURE;
    }
    else if (created == MR_SHM_CREATED)
    {
        (*d)->last_miner = -1;
    }

    (*d)->monitor_pid = getpid();

    return EXIT_SUCCESS;
}


/**
 * @brief 
 * 
 * @param block 
 * @param queue 
 * @return int 1 in case of error. 0 otherwise. 
 */
int mr_mq_receive(Block *block, mqd_t queue)
{
    if (mq_receive(queue, (char *)block, sizeof(*block), NULL) == -1)
    {
        if (errno != EINTR)
            perror("mq_receive");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/**
 * @brief Comprueba si el bloque b está ya en blocks. 
 * Si está, comprueba que la target y la solución coincida, y 
 * devuelve 1. Si no está, lo añade y devuelve 0. En caso de error, 
 * *err se pone a 1 y se devuelve 1;
 * 
 * @param b 
 * @param blocks 
 * @param err puntero cuyo valor se establece a 1 en caso de error
 * @return int 0 si b no está en blocks y se añade con éxito.
 * 1 en caso contrario (o error). 
 * En caso de error, *err se pone a 1. 
 */
int mr_monitor_block_is_repeated(Block *b, Monitor_blocks *blocks, int *err)
{
    int i, id;
    long int solution, target;
    id = b->id;

    for (i = 0; i < MAX_BLOCKS && blocks->buffer[i].id != id; i++);

    if (i < MAX_BLOCKS)
    { /* repetido */
        target = blocks->buffer[i].target;
        solution = blocks->buffer[i].solution;
        if (b->target == target && b->solution == solution)
        {
            printf("Verified block %i with solution %ld for target %ld \n", id, solution, target);
        }
        else
        {
            printf("Error in block %i with solution %ld for target %ld \n", id, solution, target);
        }
        return 1;
    }
    
    blocks->last_block = ((blocks->last_block) + 1) % MAX_BLOCKS;

    if ((memcpy(&(blocks->buffer[blocks->last_block]), b, sizeof(Block))) == NULL)
    {
        *err = 1;
        return 1;
    }

    return 0;
}

/**
 * @brief 
 * 
 * @param block 
 * @param fd 
 * @return int 
 */
int mr_fd_read_block(Block *block, int fd[2], Block* last_block, int n_wallets, int file, int *err)
{
    int total_size_read = 0;
    int target_size = sizeof(Block);
    int size_read = 0;

    do
    {
        size_read = read(fd[0], ((char *)block) + total_size_read, target_size - total_size_read);
        if (size_read == -1)
        {
            if(errno == EINTR && got_sigalrm)
                (*err) = mr_printer_handle_sigalrm(last_block, n_wallets, file);
            else if(errno != EINTR)
            {
                perror("read");
                return -1;
            }
        }
        else if (size_read == 0)
            return total_size_read;
        else
            total_size_read += size_read;

    } while (total_size_read != target_size && !(*err));

    return (*err) ? 0 : total_size_read;
}

int mr_fd_write_block(Block *block, int fd[2])
{

    int total_size_written = 0;
    int target_size = sizeof(Block);
    int size_written = 0;
    
    do
    {
        size_written = write(fd[1], ((char *)block) + total_size_written, target_size - total_size_written);
        if (size_written == -1 && errno != EINTR)
        {
            perror("write");
            return 1;
        }
        else if (size_written != -1)
            total_size_written += size_written;
    } while (total_size_written != target_size);

    return 0;
}


int mr_printer_handle_sigalrm(Block *last_block, int n_wallets, int file)
{
    got_sigalrm = 0;
    if (alarm(PRINTER_WAIT))
    {
        fprintf(stderr, "Existe una alarma previa establecida\n");
        return 1;
    }
    print_blocks_file(last_block, n_wallets, file);
    return 0;
}

void mr_monitor_close_net_mutex(sem_t *mutex, NetData* s_net_data)
{
    while(sem_wait(mutex) == -1);
    s_net_data->monitor_pid = -2;

    if(!(s_net_data->num_active_miners))
    {
        printf("destroy everything\n");
        sem_destroy(&(s_net_data->sem_round_begin));
        sem_destroy(&(s_net_data->sem_round_end));
        sem_destroy(&(s_net_data->sem_scrutinizing));
        sem_destroy(&(s_net_data->sem_start_voting));
        sem_destroy(&(s_net_data->sem_votation_done));
        sem_unlink(SEM_MUTEX_NAME);
        shm_unlink(SHM_NAME_BLOCK);
        shm_unlink(SHM_NAME_NET);
        sem_unlink(SEM_MUTEX_NAME);
        mq_unlink(MQ_MONITOR_NAME);
    }
    sem_post(mutex);

    sem_close(mutex);
    munmap(s_net_data, sizeof(NetData));
}