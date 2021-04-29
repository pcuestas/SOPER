#include "mr_monitor.h"
#include "mr_util.h"

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
int mr_fd_read_block(Block *block, int fd[2])
{
    int total_size_read = 0;
    int target_size = sizeof(Block);
    int size_read = 0;

    do
    {
        size_read = read(fd[0], ((char *)block) + total_size_read, target_size - total_size_read);
        if (size_read == -1 && errno != EINTR)
        {
            perror("read");
            return -1;
        }
        else if (size_read == 0)
            return total_size_read;
        else if (size_read != -1)
            total_size_read += size_read;

    } while (total_size_read != target_size);

    return total_size_read;
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

void mr_monitor_printer_print_blocks(Block *last_block, int file)
{
    int i;

    if(last_block == NULL)
    {
        print_blocks_file(last_block, 1, file);
        return ;
    }

    /*encuentra la primera wallet no vacía*/
    for(i = MAX_MINERS - 1; (i >= 0) && !(last_block->wallets[i]); --i);

    print_blocks_file(last_block, i + 1, file);
}