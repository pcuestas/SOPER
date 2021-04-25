#include "miner.h"
#include "mr_miner.h"
#include "mr_monitor.h"

static volatile int got_sigint = 0;
static volatile int got_sigalrm = 0;

void handler_sigint(int sig)
{
    got_sigint = 1;
}

void handler_sigalrm(int sig)
{
    got_sigalrm = 1;
}


int mr_shm_init_monitor(NetData **d)
{
    int created;

    created = mr_shm_map(SHM_NAME_NET, (void**)d, sizeof(NetData));

    if(created == MR_SHM_FAILED)
    {
        return EXIT_FAILURE;
    }
    else if(created == MR_SHM_CREATED)
    {
        (*d)->last_miner = -1;
    }
    
    (*d)->monitor_pid = getpid();

    return EXIT_SUCCESS;
}


void print_blocks_file(Block *plast_block, int num_wallets,int fd) {
    Block *block = NULL;
    int i, j;

    lseek(fd, 0, SEEK_SET);// error ? -1

    for(i = 0, block = plast_block; block != NULL; block = block->prev, i++) {
        dprintf(fd,"Block number: %d; Target: %ld;    Solution: %ld\n", block->id, block->target, block->solution);
        for(j = 0; j < num_wallets; j++) {
            dprintf(fd,"%d: %d;         ", j, block->wallets[j]);
        }
        dprintf(fd,"\n\n\n");
    }
    dprintf(fd,"A total of %d blocks were printed\n", i);
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
    int file;
    struct sigaction act;
    int ret, err = 0;
    sigset_t mask;
    close(fd[1]);

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, NULL);
     
    sigemptyset(&(act.sa_mask));
    act.sa_handler = handler_sigalrm;
    act.sa_flags = 0;

    if (sigaction(SIGALRM, &act, NULL) < 0){
        perror("sigaction SIGALRM");
        close(fd[0]);
        exit(EXIT_FAILURE);
    }

    if ((file = open(LOG_FILE, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1){
        perror("open");
        close(fd[0]);
        exit(EXIT_FAILURE);
    }

    if (alarm(PRINTER_WAIT)) {
        close(fd[0]);
        close(file);
        fprintf(stderr, "Existe una alarma previa establecida\n");
        err = 1;
    }

    while(!err && (ret = mr_fd_read_block(&block, fd) > 0)){
        printf("entra-printer\n");
        last_block = mr_shm_block_copy(&block, last_block);
        printf("Bloque copiado %i %ld %ld\n", last_block->id, last_block->target, last_block->solution);
        if(last_block == NULL)
            err = 1;
        if(!err && got_sigalrm){
            got_sigalrm = 0; 
            if (alarm(PRINTER_WAIT)) {
                fprintf(stderr, "Existe una alarma previa establecida\n");
            }
            /**
            printf("escribo\n");
            print_blocks(last_block, 1);
            print_blocks_file(last_block, 1, file);  */
        }
    }

 
    print_blocks_file(last_block, 1, file); 
    mr_blocks_free(last_block);

    close(fd[0]);
    close(file);

    if(err || ret < 0){
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
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
    if(mq_receive(queue, (char*)block, sizeof(*block), NULL) == -1)
    {   
        if(errno !=  EINTR)
            perror("mq_receive");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


int block_is_repeated(Block *b, Monitor_blocks *blocks, int *err){
    int i,id;
    long int solution, target;
    id=b->id;

    for(i = 0 ; i < MAX_BLOCKS && blocks->buffer[i].id != id; i++);
    
    if (i < MAX_BLOCKS){/* repetido */
        target = blocks->buffer[i].target;
        solution = blocks->buffer[i].solution;
        if (b->target == target && b->solution == solution){
            printf("Verified block %i with solution %ld for target %ld \n",id,solution,target);
        }
        else
        {
           printf("Error in block %i with solution %ld for target %ld \n",id,solution,target);
        }
        return 1;
    }
    printf("add block: %dd\n", b->id);

    blocks->last_block = ((blocks->last_block) + 1)%MAX_BLOCKS;

    if((memcpy(&(blocks->buffer[blocks->last_block]), b, sizeof(Block))) == NULL)
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
int mr_fd_read_block(Block *block, int fd[2]){
    int total_size_read = 0;
    int target_size = sizeof(Block);
    int size_read = 0;    
    
    do {
        size_read = read(fd[0], ((char*)block) + total_size_read, target_size - total_size_read);
        if (size_read  ==  -1 && errno != EINTR) 
        {
            perror("read");
            return -1;
        }
        else if(size_read == 0)
            return total_size_read;
        else if(size_read != -1)
            total_size_read  +=  size_read;

    } while(total_size_read  !=  target_size);

    return total_size_read;
}


int mr_fd_write_block(Block *block, int fd[2]){

    int total_size_written = 0;
    int target_size = sizeof(Block);
    int size_written = 0;
    printf("escribo: %d\n", block->id);
    //write(fd[1], block, target_size);
    
    do {
        size_written = write(fd[1], ((char*)block) + total_size_written, target_size - total_size_written);
        if (size_written == -1 && errno != EINTR) {
            perror("write");
            return 1;
        }else if(size_written != -1)
            total_size_written  +=  size_written;
    } while(total_size_written  !=  target_size);
    
    return 0;
}



int main(int argc, char *argv[])
{

    int fd[2], err = 0, i;
    pid_t child;
    mqd_t queue;
    Monitor_blocks blocks;
    Block block;
    NetData *s_net_data;
    sem_t *mutex;
    struct sigaction act;

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

    if (sigaction(SIGINT, &act, NULL) < 0){
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

    while(sem_wait(mutex) == -1);
    if (mr_shm_init_monitor(&s_net_data) == EXIT_FAILURE)
    {
        exit(EXIT_FAILURE);
    }
    sem_post(mutex);

    queue = mr_monitor_mq_open(MQ_MONITOR, O_CREAT | O_RDONLY);
    if (queue == (mqd_t)-1)
    {
        close(fd[1]);
        sem_close(mutex);
        exit(EXIT_FAILURE);
    }

    for(i = 0; i < MAX_BLOCKS; i++)
        blocks.buffer[i].id = -1;

    while (!err && !got_sigint)
    {
        err = mr_mq_receive(&block, queue);
        if (!err && !block_is_repeated(&block, &blocks, &err))
        { //ver que es correcto y no repetido
            err = mr_fd_write_block(&block, fd);
        }
    }

    while(sem_wait(mutex) == -1);
    s_net_data->monitor_pid = -2;
    sem_post(mutex);

    munmap(s_net_data, sizeof(NetData));

    close(fd[1]);
    wait(NULL);
    printf("sale correctamente\n");
    exit(EXIT_SUCCESS);
}
