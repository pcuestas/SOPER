/**
 * @file mr_miner_workers.c (Proyecto SOPER)
 * @author Pablo Cuesta Sierra, Álvaro Zamanillo Sáez 
 * 
 * @brief Fichero con la implemetación de las funciones 
 * relacionadas con los trabajadores de los mineros. 
 * Estas funciones están declaradas en mr_miner.h
 */
#include "miner.h"
#include "mr_miner.h"

extern int end_threads;
extern long int proof_solution;

/**
 * @brief función para el hilo del trabajador. 
 * Solución al target en la estructura empezando por 
 * el valor inicial y terminando en el final que pone en su 
 * estructura pasada por d.
 * Si encuentra solución, lo comunica a los demás trabajadores 
 * poniendo la variable global end_threads a 1 y la variable
 * proof_solution con el valor de la solución. Y manda SIGHUP
 * al propio proceso para señalizar que es ganador.
 * 
 * @param d estructura de tipo WorkerStruct 
 * @return NULL en todo caso
 */
void *mrw_thread_mine(void *d)
{
    WorkerStruct *data = (WorkerStruct *)d;
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

/**
 * @brief inicializa n_workers estructuras de tipo WorkerStruct
 * en un array, alocando memoria para ello y estableciendo 
 * los valores iniciales y final de cada trabajador
 * 
 * @param n_workers número de trabajadores
 * @return WorkerStruct* el array reservado e inicializado
 */
WorkerStruct *mrw_struct_init(int n_workers)
{
    int i;
    long int interval = PRIME / n_workers;
    WorkerStruct *workers_struct_arr;

    workers_struct_arr = (WorkerStruct *)calloc(n_workers, sizeof(WorkerStruct));
    if (workers_struct_arr == NULL)
    {
        perror("calloc");
        return NULL;
    }

    workers_struct_arr[0].begin = 0;
    for (i = 1; i < n_workers; i++)
    {
        workers_struct_arr[i - 1].end = workers_struct_arr[i].begin = i * interval;
    }
    workers_struct_arr[n_workers - 1].end = PRIME;

    return workers_struct_arr;
}

/**
 * @brief lanza los hilos de los trabajadores
 * 
 * @param workers array de los hilos que se lanzan
 * @param workers_struct_arr array de las estructuras de los mineros
 * @param n_workers número de trabajadores
 * @param target nuevo target
 * @return EXIT_SUCCESS en caso de éxito, EXIT_FAILURE si hay algún error 
 */
int mrw_launch(pthread_t *workers, WorkerStruct *workers_struct_arr, int n_workers, long int target)
{
    int i, j, err = 0;
    end_threads = 0;

    for (i = 0; i < n_workers && !err; i++)
    {
        workers_struct_arr[i].target = target;
        err = pthread_create(&workers[i], NULL, mrw_thread_mine, (void *)&(workers_struct_arr[i]));

        if (err)
        {
            errno = err;
            perror("pthread_create");
        }
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

/**
 * @brief Hace join a todos los hilos trabajadores
 * 
 * @param workers array con los hilos
 * @param n_workers número de trabajadores
 */
void mrw_join(pthread_t* workers, int n_workers)
{
    int i;
    end_threads = 1;

    for(i = 0; i < n_workers; i++)
    {   
        pthread_join(workers[i], NULL);
    }   
}