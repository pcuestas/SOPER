#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

/**
 * Implementación de una shell sencilla - sin redirecciones ni estructuras de control.
 * Con registro de los comandos ejecutados y sus mensajes de salida.
 * (SOPER. Práctica 1. Ejercicio 13.)
 * 
 * @authors Pablo Cuesta Sierra, Álvaro Zamanillo Sáez 
 * 
 **/


#define BUF_SIZE 1024
#define MAX_WORDS 200
#define LOG_FILE "log.txt"

/*estructura para leer el comando*/
typedef struct Line_s_{
    char buf[BUF_SIZE];
    char *words[MAX_WORDS];
} Line_s;

/**
 * @brief función que procesa el comando leído separando las palabras. 
 * Al terminar, la estructura contiene las palabras del comando escrito 
 * en la cadena line->words, con un puntero a NULL tras la última palabra.
 * @param line estructura Line_s que contiene la línea del comando leída
 */
void *process_line(void *line){
    Line_s *l=(Line_s*)line;
    char **w, *s=" ";
    int i=0;

    /*con strtok separamos la cadena en palabras y rellenamos el array l->words*/
    w=l->words;
    for((*w)=strtok(l->buf, s); w[i]!=NULL && i<(MAX_WORDS-1); w[++i]=strtok(NULL,s));

    w[i]=(char*)NULL;/*por si se llega a i=MAX_WORDS-1*/

    return NULL;
}


int main(void){
    Line_s line;
    int err, wstatus, fd[2], file, nread;
    pthread_t h;
    pid_t pid, pid_log;
    char message[BUF_SIZE];

    if (pipe(fd)==-1){
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_log=fork(); 
    if(pid_log<0){
        perror("fork");
        exit(EXIT_FAILURE);
    }else if(pid_log==0){/*hijo que escribe en el fichero*/
        close(fd[1]);

        /*abrir el fichero para escribir los comandos y los mensajes de la salida del hijo*/
        if ((file = open(LOG_FILE, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1){
            perror("open");
            exit(EXIT_FAILURE);
        }
        
        /*leer hasta que el padre cierre fd[1]*/
        while((nread=read(fd[0], message, BUF_SIZE))>0){
            write(file, message, nread);
        } 
        
        close(file);
        close(fd[0]);
        exit(EXIT_SUCCESS);
    }

    close(fd[0]);/*cerrar el descriptor de entrada en el padre*/

    printf("proc_shell: $ ");fflush(stdout);

    /*bucle principal*/
    while(fgets(line.buf, BUF_SIZE, stdin)!=NULL){
        dprintf(fd[1], "%s", line.buf);/*mandar por la tubería el comando leído*/

        line.buf[strlen(line.buf)-1]='\0';/*eliminar el \n del final*/

        if((err = pthread_create(&h, NULL, process_line, &line)) != 0){
            fprintf(stderr, "pthread_create: %s\n", strerror(err));
            exit (EXIT_FAILURE);
        }
        if((err = pthread_join(h, NULL)) != 0){
            fprintf(stderr, "pthread_join: %s\n", strerror(err));
            exit (EXIT_FAILURE);
        }
        
        pid=fork();
        if(pid<0){
            perror("fork");
            exit(EXIT_FAILURE);
        }else if(pid==0){
            if(execvp(line.words[0],line.words)){ /*ejecutar el comando*/
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        }
        
        if(wait(&wstatus)==-1){
            perror("wait");
            exit(EXIT_FAILURE);
        }
        if(WIFEXITED(wstatus)){
            snprintf(message, BUF_SIZE, "Exited with value: %d\n", WEXITSTATUS(wstatus));
        }else if(WIFSIGNALED(wstatus)){
            snprintf(message, BUF_SIZE, "Terminated by signal: %s\n", strsignal(WTERMSIG(wstatus)));
        }
        fprintf(stderr, "\n%s\nproc_shell: $ ", message);
        dprintf(fd[1], "%s\n", message);/*mandar el mensaje por la tubería*/
    }
    close(fd[1]);
    
    /*Esperar al hijo que escribe en log.txt*/
    if(wait(NULL)==-1){
        perror("wait");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}