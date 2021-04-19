#ifndef STREAM_H_
#define STREAM_H_

#define BUFFER_SIZE 5
#define SECS_WAIT 2
#define SHM_NAME "/shm_stream"
#define MQ_SERVER "/mq_serverw"
#define MQ_CLIENT "/mq_clientw"

#include <semaphore.h>
#include <errno.h>
#include <time.h>

typedef struct stream_t
{
    char buffer[BUFFER_SIZE];
    int post_pos;
    int get_pos;
    sem_t sem_fill;
    sem_t sem_empty;
    sem_t sem_mutex;
}Stream_t;

typedef struct{
    int valor; /*1 para acción, 0 para exit*/
} Mensaje;


Stream_t *open_shmSegment(char *segmentName){
    int i, fd_shm;
    Stream_t *shm_struct;
    if ((fd_shm = shm_open(segmentName, O_RDWR, 0)) == -1){
        perror("shm_open server: ");
        exit(EXIT_FAILURE);
    }

    /* Mapping of the memory segment. */
    shm_struct = mmap(NULL, sizeof(Stream_t), PROT_WRITE | PROT_READ, MAP_SHARED, fd_shm, 0);
    close(fd_shm);
    if (shm_struct == MAP_FAILED){
        perror("mmap server: ");
        exit(EXIT_FAILURE);
    }

    return shm_struct;
}


int _sem_timedwait(sem_t *sem){
    int s;
    struct timespec ts;

    if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
    {
        perror("");
        exit(EXIT_FAILURE);
    }

    ts.tv_sec += SECS_WAIT;

    while ((s = sem_timedwait(sem, &ts)) == -1 && errno == EINTR)
    {
        continue;
    }

    if (s == -1)
    {
        if (errno == ETIMEDOUT){
            return -1;
        }else{
            perror("Sem wait");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}

mqd_t open_message_queues(char *queuname, int _oflag ){
    struct mq_attr attributes = {
        .mq_flags = 0,
        .mq_maxmsg = 10,
        .mq_curmsgs = 0,
        .mq_msgsize = sizeof(Mensaje)};

    mqd_t queue = mq_open(queuname,
                          O_CREAT | _oflag, /* This process is only going to send messages */
                          S_IRUSR | S_IWUSR,   /* The user can read and write */
                          &attributes);

    if (queue == (mqd_t)-1)
    {
        fprintf(stderr, "Error opening the queue\n");
        return EXIT_FAILURE;
    }
}


int _receive_message(mqd_t queue){
    Mensaje msg;
    if (mq_receive(queue, (char *)&msg, sizeof(msg), NULL) == -1)
    {
        fprintf(stderr, "Error receiving message\n");
        perror("");
        return EXIT_FAILURE;
    }

    return msg.valor;
}

#endif