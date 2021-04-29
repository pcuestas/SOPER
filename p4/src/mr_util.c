#include "mr_util.h"
#include "miner.h"



int mr_shm_map(char* file_name, void **p, size_t size)
{
    int ret_flag = MR_SHM_CREATED;

    /*creación del segmento de memoria compartida*/
    int fd = shm_open(file_name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd == -1)
    {
        if (errno == EEXIST)
        {
            ret_flag = MR_SHM_EXISTS;
            fd = shm_open(file_name, O_RDWR, 0);
        }
        if (fd == -1)
        {
            perror("shm_open");
            return MR_SHM_FAILED;
        }
    }
    else if (ftruncate(fd, size) == -1) /*si ha sido creado, hay que establecer el tamaño*/
    {
        perror("ftruncate");
        close(fd);
        return MR_SHM_FAILED;
    }
    
    /*mapear el segmento de memoria*/
    (*p) = mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if ((*p) == MAP_FAILED)
    {
        perror("mmap");
        return MR_SHM_FAILED;
    }

    return ret_flag;
}


/**
 * @brief abre la cola con nombre queue_name con 
 * las banderas __oflag
 * 
 * @param queue_name nombre de la cola a abrir
 * @param __oflag igual que __oflag en mq_open
 * @return la cola abierta, o (mdq_t)-1 en caso de error
 */
mqd_t mr_monitor_mq_open(char *queue_name, int __oflag)
{
    struct mq_attr attributes = {
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_curmsgs = 0,
        .mq_msgsize = sizeof(Block)
    };

    mqd_t queue = mq_open(queue_name, __oflag,
                          S_IRUSR | S_IWUSR,
                          &attributes);
    if (queue == (mqd_t)-1)
        perror("mq_open");

    return queue;
}


void mr_blocks_free(Block *last_block)
{
    Block *prev;

    while(last_block != NULL)
    {
        prev = last_block->prev;
        free(last_block);
        last_block = prev;
    }
}


Block* mr_shm_block_copy(Block *shm_b, Block *last_block){
    Block *new_block=NULL;

    if(!(new_block=(Block*)calloc(1,sizeof(Block)))){
        return NULL;
    }

    memcpy(new_block, shm_b, sizeof(Block));
    if(last_block != NULL)
        last_block->next = new_block;
    new_block->prev = last_block;

    return new_block;
}

/**
 * @brief realiza un sem_timedwait de seconds segundos 
 * en el semáforo sem. Ignora las posibles interrupciones
 * por señales y devuelve solo cuando hay error, se agota el 
 * tiempo de espera, o se baja el semáforo con éxito. 
 * En caso de algún error (o que se agote el tiempo), imprime por stderr
 * el mensaje de error correspondiente y devuelve 1;
 * 
 * @param sem semáforo en el que se realiza la espera
 * @param seconds segundos 
 * @return 1 en caso de que falle clock_gettime
 * o sem_timedwait. 0 en caso de éxito
 */
int mr_timed_wait(sem_t *sem, int seconds)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
    {
        perror("clock_gettime");
        return 1;
    }
    ts.tv_sec += seconds;
    while (sem_timedwait(sem, &ts) == -1)
    {
        if (errno == ETIMEDOUT)
        {
            fprintf(stderr, "sem_timedwait() tiempo de espera agotado. Finaliza el minado\n");
            return 1;
        }
        else if (errno != EINTR)//errno==EINTR en caso de que sea interrumpida la llamada por una señal
        {
            perror("sem_timedwait");
            return 1;
        }
    }
    return 0;
}


void print_blocks_file(Block *plast_block, int num_wallets, int fd)
{
    Block *block = NULL;
    int i, j;

    lseek(fd, 0, SEEK_SET); // error ? -1

    for (i = 0, block = plast_block; block != NULL; block = block->prev, i++)
    {
        dprintf(fd, "Block number: %d; Target: %ld;    Solution: %ld\n", block->id, block->target, block->solution);
        for (j = 0; j < num_wallets; j++)
        {
            dprintf(fd, "%d: %d;         ", j, block->wallets[j]);
        }
        dprintf(fd, "\n\n\n");
    }
    dprintf(fd, "A total of %d blocks were printed\n", i);
}