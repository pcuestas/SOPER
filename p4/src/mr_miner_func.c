/**
 * @file mr_miner_func.c (Proyecto SOPER)
 * @author Pablo Cuesta Sierra, Álvaro Zamanillo Sáez
 * @brief Implementación de las funciones del minero.
 * Declaraciones en mr_miner.h
 */
#include "miner.h"
#include "mr_miner.h"
#include "mr_util.h"
#include <stdint.h>

/**
 * @brief establece los manejadores de señales del minero
 * con la máscara mask para los handlers. Se ignoran 
 * SIGUSR1 y SIGUSR2. Se pone el manejador handler_miner
 * para SIGINT y SIGHUP.
 * 
 * @param mask máscara a utilizar en los handlers
 * @return EXIT_FAILURE en caso de error, 
 * EXIT_SUCCESS en caso de éxito
 */
int mrr_set_handlers(sigset_t mask)
{
    struct sigaction act;

    act.sa_mask = mask;
    act.sa_flags = 0;
    act.sa_handler = SIG_IGN; /* ignorar SIGUSR1 y SIGUSR2*/
    if (sigaction(SIGUSR1, &act, NULL) < 0)
    {
        perror("sigaction SIGUSR1");
        return (EXIT_FAILURE);
    }
    if (sigaction(SIGUSR2, &act, NULL) < 0)
    {
        perror("sigaction SIGUSR2");
        return (EXIT_FAILURE);
    }

    act.sa_handler = handler_miner;
    if (sigaction(SIGHUP, &act, NULL) < 0)
    {
        perror("sigaction SIGHUPINT");
        return (EXIT_FAILURE);
    }    
    if (sigaction(SIGINT, &act, NULL) < 0)
    {
        perror("sigaction SIGINT");
        return (EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

/**
 * @brief inicializar la memoria compartida, creando los segmentos
 * en caso de que no existan y mapeándolos. 
 * Si este proceso es el creador del bloque (block), se inicializan 
 * los campos de la estructura.
 * Si este proceso es el creador o si el creador ha sido el monitor y 
 * este es el primer minero en llegar, se inicializan los valores del 
 * netdata. 
 * En el caso de que este sea el primer minero activo, se 
 * inicializan los semáforos también y se declara ganador para preparar 
 * la  siguiente ronda.
 * 
 * @param b *b apuntará al bloque mapeado 
 * @param d *d apuntará a la netdata mapeada
 * @param this_index *this_index tendrá el valor del índice de este 
 * minero en el array de miners_pid y demás.
 * @return EXIT_FAILURE en caso de error, 
 * EXIT_SUCCESS en caso de éxito
 */
int mrr_shm_init(Block **b, NetData **d, int *this_index)
{
    int created, i;
    pid_t this_pid = getpid();
    srand(time(NULL));

    created = mr_shm_map(SHM_NAME_BLOCK, (void **)b, sizeof(Block));

    if (created == MR_SHM_FAILED)
        return EXIT_FAILURE;
    else if (created == MR_SHM_CREATED)
    {
        for (i = 0; i < MAX_MINERS; i++)
            (*b)->wallets[i] = 0;
        (*b)->id = -1;
        (*b)->solution = rand() % PRIME;
        (*b)->is_valid = 0;
    }

    created = mr_shm_map(SHM_NAME_NET, (void **)d, sizeof(NetData));

    if (created == MR_SHM_FAILED)
    {
        munmap(*b, sizeof(**b));
        return EXIT_FAILURE;
    }
    if ((created == MR_SHM_CREATED) || ((*d)->last_miner == -1))
    { /*si se acaba de crear o el monitor ha sido el creador del segmento*/
        for (i = 0; i < MAX_MINERS; i++)
        {
            (*d)->miners_pid[i] = -2;
            (*d)->voting_pool[i] = VOTE_NOT_VOTED;
        }
        if (created == MR_SHM_CREATED)
            (*d)->monitor_pid = -2;
        (*d)->miners_pid[0] = this_pid;
        (*d)->last_miner = 0;
        (*d)->last_winner = this_pid;
        (*d)->num_active_miners = 1;
    }
    else
    {
        (*d)->last_miner++;
        (*d)->miners_pid[(*d)->last_miner] = this_pid;
        ((*d)->num_active_miners)++;
    }

    (*this_index) = (*d)->last_miner;

    if ((*d)->num_active_miners == 1)
    {
        /*el primer minero activo siempre reestablece los semáforos y se declara ganador*/
        (*d)->last_winner = this_pid;
        if (sem_init(&((*d)->sem_round_begin), 1, 0) != 0)
        {
            perror("sem_init");
            munmap(*b, sizeof(**b));
            return EXIT_FAILURE;
        }

        if (sem_init(&((*d)->sem_round_end), 1, 1) != 0)
        {
            perror("sem_init");
            munmap(*b, sizeof(**b));
            sem_destroy(&((*d)->sem_round_begin));
            munmap(*d, sizeof(**d));
            return EXIT_FAILURE;
        }

        if (sem_init(&((*d)->sem_scrutiny), 1, 0) != 0)
        {
            perror("sem_init");
            munmap(*b, sizeof(**b));
            sem_destroy(&((*d)->sem_round_begin));
            sem_destroy(&((*d)->sem_round_end));
            munmap(*d, sizeof(**d));
            return EXIT_FAILURE;
        }

        if (sem_init(&((*d)->sem_votation_done), 1, 0) != 0)
        {
            perror("sem_init");
            munmap(*b, sizeof(**b));
            sem_destroy(&((*d)->sem_round_begin));
            sem_destroy(&((*d)->sem_round_end));
            sem_destroy(&((*d)->sem_scrutiny));
            munmap(*d, sizeof(**d));
            return EXIT_FAILURE;
        }

        if (sem_init(&((*d)->sem_start_voting), 1, 0) != 0)
        {
            perror("sem_init");
            munmap(*b, sizeof(**b));
            sem_destroy(&((*d)->sem_votation_done));
            sem_destroy(&((*d)->sem_round_begin));
            sem_destroy(&((*d)->sem_round_end));
            sem_destroy(&((*d)->sem_scrutiny));
            munmap(*d, sizeof(**d));
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

/**
 * @brief prepara los valores de los segmentos compartidos 
 * al inicio de una nueva ronda
 * 
 * @param b bloque compartido
 * @param d netdata compartida
 */
void mrr_set_round(Block *b, NetData *d)
{
    int i;

    b->target = b->solution;
    b->id++;
    b->is_valid = 0;
    b->solution = -1;

    for (i = 0; i <= d->last_miner; i++)
        d->voting_pool[i] = VOTE_NOT_VOTED;

    mrr_quorum(d);
}

/**
 * @brief manda SIGUSR1 a todos los mineros que tienen su pid en 
 * el array de net->miners_pid y actualiza el valor net->total_miners
 * con el número de mineros cuyo envío es exitoso (kill devuelve 0)
 * 
 * @param net netdata (memoria compartida)
 */
void mrr_quorum(NetData *net)
{
    int n = 1, i;
    pid_t this_pid = getpid(), pid;

    for (i = 0; i <= (net->last_miner); i++)
    {
        pid = net->miners_pid[i];
        if ((pid > 0) && (this_pid != pid))
        {
            if (!(kill(pid, SIGUSR1)))
                n++;
            else
                net->miners_pid[i] = -2;
        }
    }

    net->total_miners = n;
}

/**
 * @brief pone la solución en el bloque (compartido)
 * 
 * @param b bloque compartido
 * @param solution solución subida por este minero
 */
void mrr_set_solution(Block *b, long int solution)
{
    b->solution = solution;
}

/**
 * @brief configura las máscaras del minero. Se bloquean en el proceso
 * las señales: SIGUSR1, SIGUSR2, SUGHUP y SIGINT.
 * 
 * @param mask se guardan las señales bloqueadas por el proceso
 * @param mask_wait_workers contiene las señales bloqueadas por el proceso, 
 * excepto SIGUSR2 y SIGHUP (para que se puedan recibir cuando el minero
 * espere a que se encuantre una solución)
 * @param old_mask guarda la antigua máscara del proceso (antes de llamar 
 * a la función)
 */
void mrr_masks_set_up(sigset_t *mask, sigset_t *mask_wait_workers, sigset_t *old_mask)
{
    sigemptyset(mask);
    sigaddset(mask, SIGUSR1);
    sigaddset(mask, SIGUSR2);
    sigaddset(mask, SIGHUP);
    sigaddset(mask, SIGINT);
    sigprocmask(SIG_BLOCK, mask, old_mask);

    /*la máscara para esperar a los trabajadores tiene solo SIGUSR1*/
    (*mask_wait_workers) = (*mask);
    sigdelset(mask_wait_workers, SIGUSR2);
    sigdelset(mask_wait_workers, SIGHUP);
}

/**
 * @brief llamada por el minero ganador. 
 * Comprueba los votos
 * 
 * @param net netdata (shm)
 * @return 1 en caso de que la votación sea favorable
 * (al menos la mitad de votantes votan a favor). 
 * 0 en caso contrario.
 */
int mrr_check_votes(NetData *net)
{
    int i, count = 1; /*el ganador se vota a sí mismo*/
    char vote;

    for (i = 0; i <= (net->last_miner); i++)
    {
        vote = net->voting_pool[i];
        count += (vote == VOTE_YES);
    }

    return ((2 * count) >= net->num_voters);
}

/**
 * @brief votar la solución subida a memoria compartida. El último 
 * votante notifica al ganador la finalización de la votación. Para esto,
 * se utiliza el mecanismo "lightswitchoff"/"lightswitch unlock".
 * 
 * @param mutex mutex que protege 
 * @param net netdata (shm) para acceder a la voting_pool
 * @param b bloque (shm) para ver si el target y la solución se 
 * corresponden correctamente
 * @param this_index índice de este minero en las estructuras compartidas
 */
void mrr_vote(sem_t *mutex, NetData *net, Block *b, int this_index)
{
    int flag;

    flag = (b->target == simple_hash(b->solution));
    net->voting_pool[this_index] = flag ? VOTE_YES : VOTE_NO;

    mr_lightswitchoff(mutex, &(net->num_voters), &(net->sem_votation_done));
}

/**
 * @brief ganador avisa a los votantes que ha terminado el escrutinio
 * 
 * @param net netdata (shm)
 * @param n_voters número de votantes
 */
void mrr_send_end_scrutiny(NetData *net, int n_voters)
{
    while (n_voters--)
        sem_post(&(net->sem_scrutiny));
}

/**
 * @brief Mecanismo lightswitchoff, o lightswitch unlock.
 * El último proceso en disminuir el contador (el que llega a 0)
 * hace up del semáforo. Se protege el acceso al contador con 
 * un mutex.
 * 
 * @param mutex mutex
 * @param count puntero al contador
 * @param sem semáforo que se hace up en caso de ser el último
 */
void mr_lightswitchoff(sem_t *mutex, int *count, sem_t *sem)
{
    while (sem_wait(mutex) == -1);

    (*count)--;
    if (!(*count))
        sem_post(sem);

    sem_post(mutex);
}

/**
 * @brief imprime la cadena de bloques en un fichero llamado: [pid].log
 * 
 * @param last_block cadena de bloques
 * @param n_wallets número de wallets
 */
void mrr_print_chain(Block *last_block, int n_wallets)
{
    char file_name[1024];
    int fd;

    sprintf(file_name, "%jd.log", (intmax_t)getpid());

    if ((fd = open(file_name, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1)
    {
        perror("open");
        return;
    }

    mr_blocks_print_to_file(last_block, n_wallets, fd);
}

/**
 * @brief este minero (ganador) notifica a los perdedores
 * que ha encontrado una solución mandando la señal SIGUSR2
 * 
 * @param net netdata (shm)
 */
void mrr_notify_miners(NetData *net)
{
    int i;
    pid_t this_pid = getpid(), pid;

    for (i = 0; i <= net->last_miner; i++)
    {
        pid = net->miners_pid[i];
        if ((this_pid != pid) && (pid > 0))
            kill(pid, SIGUSR2);
    }
}

/**
 * @brief el último ganador prepara una nueva ronda (calcula el quorum)
 * y avisa a todos los mineros para que comiencen la ronda con el semáforo
 * s_net_data->sem_round_begin 
 * 
 * @param s_block bloque (shm)
 * @param s_net_data netdata (shm)
 */
void mrr_last_winner_prepare_round(Block *s_block, NetData *s_net_data)
{
    int n_proc, i;
    /*Preparar bloque y contar numero de mineros*/
    mrr_set_round(s_block, s_net_data);
    n_proc = s_net_data->total_miners - 1;

    /*Avisar de que se inicia la ronda*/
    for (i = 0; i < n_proc; i++)
    {
        sem_post(&(s_net_data->sem_round_begin));
    }
}

/**
 * @brief acciones del ganador de una ronda:
 * Notifica a los perdedores que se ha encontrado solución.
 * Les indica que pueden empezar a votar.
 * Si hay votación, actualiza el bloque tanto en caso de que se vote 
 * que sí, como que no. Si no la hay, lo actualiza de todas formas.
 * Impide el paso de nuevos mineros a la ronda (para evitar que los
 * ninguno empiece antes de que todos hayan acabado la ronda)
 * Por último, avisa al resto de que se ha acabado el escrutinio 
 * (sem_scrutiny).
 * 
 * @param s_block block (shm)
 * @param s_net_data netdata (shm)
 * @param this_index índice del minero en la memoria compartida
 * @return 1 en caso de error (tiempo de espera agotado),
 * 0 en caso contrario
 */
int mrr_winner_actions(Block *s_block, NetData *s_net_data, int this_index)
{
    int n_voters, i;

    n_voters = (s_net_data->total_miners) - 1;
    s_net_data->num_voters = n_voters;

    mrr_notify_miners(s_net_data); /*mandar sigusr2*/

    for (i = 0; i < n_voters; i++)
    {
        sem_post(&(s_net_data->sem_start_voting));
    }

    if (!n_voters)
        mrr_winner_update_after_votation(s_block, s_net_data, this_index);
    else
    {
        if ((mr_timed_wait(&(s_net_data->sem_votation_done), 3)))
            return 1;

        if (mrr_check_votes(s_net_data))
            mrr_winner_update_after_votation(s_block, s_net_data, this_index);
        else
            /*se ha votado que el bloque no es válido*/
            mrr_loser_modify_block(s_block);
    }

    /*para que no entren mineros en el bucle hasta que el último haya terminado:*/
    if ((mr_timed_wait(&(s_net_data->sem_round_end), 3)))
        return 1;

    if (n_voters)
        mrr_send_end_scrutiny(s_net_data, n_voters);

    return 0;
}

/**
 * @brief actualiza la memoria compartida tras una votación favorable
 * 
 * @param s_block block (shm)
 * @param s_net_data netdata (shm)
 * @param this_index índice del minero en la memoria compartida
 */
void mrr_winner_update_after_votation(Block* s_block, NetData* s_net_data, int this_index)
{
    s_block->is_valid = 1;
    s_net_data->last_winner = getpid();
    (s_block->wallets[this_index])++;
}

/**
 * @brief cierra el mutex y el mapeo de la memoria compartida.
 * Si el minero es el último minero activo, destruye los semáforos 
 * de la memoria compartida. Si además no hay monitor activo, hace 
 * unlink de la cola de mensajes, de el mutex y de los segmentos
 * de la memoria compartida
 * 
 * @param mutex mutex
 * @param s_net_data netdata (shm)
 */
void mrr_close_net_mutex(sem_t *mutex, NetData *s_net_data)
{
    while (sem_wait(mutex) == -1);
    (s_net_data->num_active_miners)--;

    if (!(s_net_data->num_active_miners))
    { /*el último minero destruye los semáforos*/
        sem_destroy(&(s_net_data->sem_round_begin));
        sem_destroy(&(s_net_data->sem_round_end));
        sem_destroy(&(s_net_data->sem_scrutiny));
        sem_destroy(&(s_net_data->sem_start_voting));
        sem_destroy(&(s_net_data->sem_votation_done));
        if (s_net_data->monitor_pid < 0)
        { /*si no hay más mineros ni monitor, hacer unlink*/
            printf("destroy everything\n");
            shm_unlink(SHM_NAME_BLOCK);
            shm_unlink(SHM_NAME_NET);
            sem_unlink(SEM_MUTEX_NAME);
            mq_unlink(MQ_MONITOR_NAME);
        }
    }
    sem_post(mutex);

    sem_close(mutex);
    munmap(s_net_data, sizeof(NetData));
}

/**
 * @brief minero actualiza su cadena con el nuevo bloque válido
 * y lo manda al monitor (en caso de que esté activo)
 * 
 * @param last_block cadena de bloques
 * @param s_block nuevo bloque válido (shm)
 * @param s_net_data netdata (shm)
 * @param queue cola de mensajes
 * @param winner 1 si el minero es ganador, 0 si no
 * @return 1 en caso de error. 0 en caso contrario
 */
int mrr_valid_block_update(Block **last_block, Block *s_block, NetData *s_net_data, mqd_t queue, int winner)
{
    /*Añadir bloque correcto a la cadena de cada minero*/
    (*last_block) = mr_block_append(s_block, *last_block);

    if ((*last_block) == NULL)
        return 1;

    /*Si hay monitor, mandar el nuevo bloque*/
    if ((s_net_data->monitor_pid > 0) &&
        (mq_send(queue, (char *)(*last_block), sizeof(Block), 1 + winner) == -1))
    {
        perror("mq_send");
        return 1;
    }
    return 0;
}

/**
 * @brief minero está en su última ronda, quita su pid del array
 * de pids de mineros.
 * Si es ganador, elige otro ganador para que prepare la siguiente ronda.
 * 
 * @param mutex mutex que protege el acceso a la memoria
 * @param s_net_data netdata (shm)
 * @param this_index índice de este minero en los arrays compartidos
 */
void mrr_last_round(sem_t *mutex, NetData *s_net_data, int this_index)
{
    int i;

    while (sem_wait(mutex) == -1);

    s_net_data->miners_pid[this_index] = -2;
    if (getpid() == s_net_data->last_winner)
    {
        for (i = 0; i <= s_net_data->last_miner; i++)
        {
            if (s_net_data->miners_pid[i] > 0)
            {
                s_net_data->last_winner = s_net_data->miners_pid[i];
                break;
            }
        }
    }
    sem_post(mutex);
}

/**
 * @brief un minero que se declara ganador pero es votado 
 * que el bloque no ees válido, actualiza el bloque para que
 * la siguiente ronda se configure correctamente
 * 
 * @param s_block bloque (shm)
 */
void mrr_loser_modify_block(Block* s_block)
{
    s_block->id--;
    s_block->solution = s_block->target;
}