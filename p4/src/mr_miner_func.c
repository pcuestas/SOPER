#include "miner.h"
#include "mr_miner.h"


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


int mr_shm_init_miner(Block **b, NetData **d, int *this_index)
{
    int created, i;
    pid_t this_pid = getpid();
    srand(time(NULL));

    created = mr_shm_map(SHM_NAME_BLOCK, (void**)b, sizeof(Block));

    if(created == MR_SHM_FAILED)
        return EXIT_FAILURE;
    else if(created == MR_SHM_CREATED)
    {
        for(i = 0; i < MAX_MINERS; i++)
            (*b)->wallets[i] = 0;
        (*b)->id = -1;
        (*b)->solution = rand()%PRIME;
        (*b)->is_valid = 0;
    }

    created = mr_shm_map(SHM_NAME_NET, (void**)d, sizeof(NetData));

    if(created == MR_SHM_FAILED)
    {
        munmap(*b, sizeof(**b));
        return EXIT_FAILURE;
    }// if just created or the creator was the monitor:
    if(created == MR_SHM_CREATED || ((*d)->last_miner == -1))
    {   
        for(i = 0; i < MAX_MINERS; i++)
        {
            (*d)->miners_pid[i] = -2;
            (*d)->voting_pool[i] = VOTE_NOT_VOTED;
        }
        if(created == MR_SHM_CREATED)
            (*d)->monitor_pid = -2;
        (*d)->miners_pid[0] = this_pid;
        (*d)->last_miner = 0;
        (*d)->last_winner = this_pid;
    }
    else
    {   
        (*d)->last_miner++;
        (*d)->miners_pid[(*d)->last_miner] = this_pid;
    }

    (*this_index) = (*d)->last_miner; 

    return EXIT_SUCCESS;
}

void mr_shm_set_new_round(Block *b, NetData *d){
    int i;
    
    b->target = b->solution;
    b->id++;
    b->is_valid = 0;
    b->solution = -1;

    for(i = 0; i < d->last_miner; i++)
        d->voting_pool[i] = VOTE_NOT_VOTED;

    //Que hacer con los wallets
}


void mr_shm_quorum(NetData *net){
    int n = 1, i;
    pid_t this_pid = getpid();

    for(i = 0 ; i <= (net->last_miner); i++){
        if(this_pid != net->miners_pid[i])
            n += (!(kill(net->miners_pid[i], SIGUSR1)));
    }
    
    net->total_miners = n;
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

void mr_shm_set_solution(Block *b, long int solution){
    b->solution = solution;
}

void mr_masks_set_up(sigset_t *mask, sigset_t *mask_sigusr1, sigset_t *mask_sigusr2, sigset_t *old_mask)
{
    sigemptyset(mask);
    sigaddset(mask, SIGUSR1);
    sigaddset(mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, mask, old_mask);

    (*mask_sigusr1) = (*mask);
    sigdelset(mask_sigusr1, SIGUSR1);

    (*mask_sigusr2) = (*mask);
    sigdelset(mask_sigusr2, SIGUSR2);
}