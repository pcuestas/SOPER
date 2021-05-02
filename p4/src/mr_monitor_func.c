/**
 * @file mr_monitor_func.c (Proyecto SOPER)
 * @author Pablo Cuesta Sierra, Álvaro Zamanillo Sáez
 * @brief Implementación de las funciones del monitor.
 * Declaraciones en mr_monitor.h
 */
#include "mr_monitor.h"
#include "mr_util.h"


extern volatile sig_atomic_t got_sigalrm;
extern volatile sig_atomic_t got_sigint;

/**
 * @brief manejador de SIGINT. 
 * Pone got_sigint a 1
 * 
 * @param sig señal recibida
 */
void handler_sigint(int sig)
{
    got_sigint = 1;
}

/**
 * @brief manejador de SIGALRM. 
 * Pone got_sigalrm a 1
 * 
 * @param sig señal recibida
 */
void handler_sigalrm(int sig)
{
    got_sigalrm = 1;
}

/**
 * @brief mapea e inicializa la memoria compartida 
 * NetData (con nombre SHM_NAME_NET)
 * 
 * @param d *d contiene al terminar la función el puntero 
 * a la memoria compartida mapeada
 * @return EXIT_FAILURE en caso de error. EXIT_SUCCESS en caso de éxito.
 */
int mrt_shm_init(NetData **d)
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
    else if ((*d)->monitor_pid > 0)
    {   /*si hay ya un monitor, salir*/
        munmap((*d), sizeof(NetData));
        return EXIT_FAILURE;
    }

    (*d)->monitor_pid = getpid();

    return EXIT_SUCCESS;
}


/**
 * @brief recibir un mensaje de la cola
 * 
 * @param block apunta a donde se guarda el mensaje que se va a leer
 * @param queue cola de la que se lee el mensaje
 * 
 * @return 1 si hay error (o si se recibe señal mientras se espera a mensaje)
 * @return 0 en caso de éxito. 
 */
int mrt_mq_receive(Block *block, mqd_t queue)
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
 * devuelve 1. 
 * Si no está, lo añade y devuelve 0. En caso de error, 
 * *err se pone a 1 y se devuelve 1;
 * 
 * @param b bloque a comprobar
 * @param blocks estructura de bloques del monitor donde se comprueba
 * @param err puntero cuyo valor se establece a 1 en caso de error
 * 
 * @return 0 si b no está en blocks y se añade con éxito.
 * @return 1 en caso contrario (o error - En caso de error, *err se pone a 1.
 */
int mrt_block_is_repeated(Block *b, MonitorBlocks *blocks, int *err)
{
    int i, id;
    long int solution, target;
    id = b->id;

    for (i = 0; (i < MAX_BLOCKS) && (blocks->buffer[i].id != id); i++);

    if (i < MAX_BLOCKS)
    { /* repetido */
        target = blocks->buffer[i].target;
        solution = blocks->buffer[i].solution;
        if ((b->target == target) && (b->solution == solution))
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
 * @brief lee un bloque entero de la tubería fd ignorando posibles
 * interrupciones por señales, excepto SIGALRM, en caso de recibirla,
 *  se maneja (llamando a mrp_handle_sigalrm) y se continúa la lectura
 * 
 * @param block bloque a leer
 * @param fd tubería
 * @param last_block cadena de bloques del proceso 
 * @param n_wallets número de wallets
 * @param file fichero al que se imprime cada vez que se recibe SIGALRM
 * @param err *err toma el valor 1 en caso de error
 * @return número de bytes leídos
 * ó 0 en caso de que se lea EOF o de que haya error 
 */
int mrp_fd_read_block(Block *block, int fd[2], Block* last_block, int n_wallets, int file, int *err)
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
                (*err) = mrp_handle_sigalrm(last_block, n_wallets, file);
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

/**
 * @brief escribe un bloque entero de la tubería fd ignorando posibles
 * interrupciones por señales
 * 
 * @param block bloque que se escribe 
 * @param fd tubería
 * 
 * @return 1 en caso de error
 * @return 0 en caso de éxito
 */
int mrt_fd_write_block(Block *block, int fd[2])
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

/**
 * @brief crea una nueva alarma para dentro de PRINTER_WAIT 
 * segundos e imprime la cadena de bloques en el fichero
 * solamente si n_wallets es distinto de 0 (> 0)
 * 
 * @param last_block cadena de bloques
 * @param n_wallets número de wallets
 * @param file descriptor del fichero en el que se escribe
 * 
 * @return 1 en caso de error
 * @return 0 en caso de éxito
 */
int mrp_handle_sigalrm(Block *last_block, int n_wallets, int file)
{
    got_sigalrm = 0;
    if (alarm(PRINTER_WAIT))
    {
        fprintf(stderr, "Existe una alarma previa establecida\n");
        return 1;
    }
    if(n_wallets)
        mr_blocks_print_to_file(last_block, n_wallets, file);
    return 0;
}

/**
 * @brief libera el mutex y la memoria compartida del monitor.
 * En caso de ser el último proceso enganchado a la memoria, 
 * hace unlink del mutex, los segmentos de memoria compartida del 
 * proceso y la cola de mensajes
 * 
 * @param mutex mutex
 * @param s_net_data segmento compartido 
 */
void mrt_close_net_mutex(sem_t *mutex, NetData* s_net_data)
{
    while(sem_wait(mutex) == -1);
    s_net_data->monitor_pid = -2;

    if(!(s_net_data->num_active_miners))
    {   /*borrar segmentos de memoria compartida si no quedan mineros activos*/
        printf("destroy everything\n");
        sem_unlink(SEM_MUTEX_NAME);
        shm_unlink(SHM_NAME_BLOCK);
        shm_unlink(SHM_NAME_NET);
        mq_unlink(MQ_MONITOR_NAME);
    }
    sem_post(mutex);

    sem_close(mutex);
    munmap(s_net_data, sizeof(NetData));
}