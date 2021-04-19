#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <mqueue.h>
#include "stream.h"

#define SHM_NAME "/shm_stream"


/**
 * Función que lanza dos procesos hijos. Uno que ejecuta stream-server y otro, stream-client
 */
void init_stream_proccess(char * readfile, char* writefile){
    pid_t pid;

    pid=fork();

    /*No hace falta mandar SHM_NAME*/
    if(pid<0){
        perror("fork");
        exit(EXIT_FAILURE);
    }else if(pid==0){
        if (execlp("./stream-server", "./stream-server",readfile,SHM_NAME, (char *)NULL)){
            perror("execvp 1");
            exit(EXIT_FAILURE);
        }
    }
    printf("Priemr proceso: %i \n",pid);
    pid = fork();

    if (pid < 0){
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0){
        if (execlp("./stream-client", "./stream-client",writefile, SHM_NAME,(char *)NULL))
        {
            perror("execvp 2");
            exit(EXIT_FAILURE);
        }
    }
    printf("Segundo proceso: %i \n", pid);
}

/**
 * Crea un segmento de memoria compartido.
 */
Stream_t* init_shm_segment(){
    shm_unlink(SHM_NAME);
    int fd_shm;
    Stream_t *shm_struct;


    //Sustituir estas lineas por open_shm_segment
    if ((fd_shm = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)) == -1){
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    if (ftruncate(fd_shm, sizeof(Stream_t)) == -1){
        perror("ftruncate");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    shm_struct = mmap(NULL, sizeof(Stream_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd_shm, 0);

    close(fd_shm);
    if (shm_struct == MAP_FAILED){
        perror("mmap");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }
    //Hasta aqui


    shm_struct->get_pos=0;
    shm_struct->post_pos=0;

   /*Init semphores*/

    if(sem_init(&(shm_struct->sem_empty),1,BUFFER_SIZE)==-1){
        perror("sem init: ");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    if (sem_init(&(shm_struct->sem_mutex), 1, 1) == -1){
        perror("sem init: ");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    if (sem_init(&(shm_struct->sem_fill), 1, 0) == -1){
        perror("sem init: ");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    return shm_struct;
}



int main(int argc, char* args[]){

    int val,fd_shm,check=1;
    char buf[10];
    Stream_t *shm_struct=NULL;
    mqd_t queue_server,queue_client,queue;
    Mensaje msg;

    if(argc!=3){
        fprintf(stderr,"Two arguments needed.\n./stream-ui <READ_FILE> <WRITE_FILE>");
        return 0;
    }

    /*Creamos el segmento de memoria compartido*/
    shm_struct=init_shm_segment();

    /*Crear dos colas de mensajes*/
    queue_server=open_message_queues(MQ_SERVER,O_WRONLY);
    queue_client = open_message_queues(MQ_CLIENT, O_WRONLY);

    /*Lanzar los dos procesos stream-server y stream-client*/
    init_stream_proccess(args[1],args[2]);
    

    /*Bucle principal*/
    /*Mejorar estrucutra interna*/
    while (fgets(buf, sizeof(buf), stdin) != NULL){
        if (buf[strlen(buf) - 1]=='\n')
            buf[strlen(buf) - 1] = '\0';

        if (strcmp(buf, "exit") == 0){
            msg.valor=0;
            if (mq_send(queue_client, (char *)&msg, sizeof(msg), 1) == -1){
                fprintf(stderr, "Error sending message\n");
                exit(EXIT_FAILURE);
            }
            if (mq_send(queue_server, (char *)&msg, sizeof(msg), 1) == -1)
            {
                fprintf(stderr, "Error sending message\n");
                exit(EXIT_FAILURE);
            }
            break;
        }


        if (strcmp(buf, "get") == 0){
            msg.valor = 1;
            queue=queue_client;
        }else if (strcmp(buf, "post") == 0){
            msg.valor = 1;
            queue=queue_server;
        }else{
            printf("Write <get>, <post> or <exit> \n");
            check=0;
        }


        if(check==1){
            if(mq_send(queue,(char*)&msg,sizeof(msg),1)==-1){
                fprintf(stderr,"Error sending message\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    /*Habria que ver si he salido de fgets por NULL, en ese caso hay q hacer que terminen los procesos*/

        /*Esperar a los dos procesos hijo*/
        wait(NULL);
        wait(NULL);

        /*Liberar memoria y semaforos*/
        sem_destroy(&(shm_struct->sem_empty));
        sem_destroy(&(shm_struct->sem_fill));
        sem_destroy(&(shm_struct->sem_mutex));

        munmap(shm_struct, sizeof(Stream_t));
        mq_close(queue_client);
        mq_close(queue_server);
        mq_unlink(MQ_SERVER);
        mq_unlink(MQ_CLIENT);

        return 0;
    }