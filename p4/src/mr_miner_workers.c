#include "miner.h"
#include "mr_miner.h"

extern int end_threads;
extern long int proof_solution;

void *mine(void *d)
{
    Mine_struct *data = (Mine_struct *)d;
    long int i;

    for (i = data->begin; i < data->end && !end_threads; i++)
    {
        if (data->target == simple_hash(i))
        {
            end_threads = 1;
            proof_solution = i;
            kill(getpid(), SIGHUP);
        }
    }
    return NULL;
}

Mine_struct *mr_mine_struct_init(int n_workers)
{
    int i;
    long int interval = PRIME / n_workers;
    Mine_struct *mine_struct = (Mine_struct *)calloc(n_workers, sizeof(Mine_struct));

    if (mine_struct == NULL)
        return NULL;

    mine_struct[0].begin = 0;
    for (i = 1; i < n_workers; i++)
    {
        mine_struct[i - 1].end = mine_struct[i].begin = i * interval;
    }
    mine_struct[n_workers - 1].end = PRIME;

    return mine_struct;
}

int mr_workers_launch(pthread_t* workers, Mine_struct* mine_struct, int nWorkers, long int target)
{
    int i, j, err = 0;
    end_threads = 0;

    for (i = 0; i < nWorkers && !err; i++)
    {
        mine_struct[i].target = target;
        err = pthread_create(&workers[i], NULL, mine, (void *)&(mine_struct[i]));
        if (err)
            fprintf(stderr, "pthread_create: %d", err);
    }

    if (err)
    {
        end_threads = 1;
        for (j = 0; j < i; j++)
            pthread_join(workers[j], NULL);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void mr_workers_join(pthread_t* workers, int n_workers)
{
    int i;
    end_threads = 1;
    for(i = 0; i < n_workers; i++) 
        pthread_join(workers[i], NULL);
}