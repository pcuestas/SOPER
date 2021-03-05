#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>


#define BUF_SIZE 1024
#define MAX_WORDS 200
#define LOG_FILE "log.txt"


/*estructura para leer el comando*/
typedef struct Line_s_{
    char buf[BUF_SIZE];
    char *words[MAX_WORDS];
} Line_s;

void *proc_line(void *line){
    Line_s *l=(Line_s*)line;
    char *a, *s=" ";
    int i=0;

    for(a=strtok(l->buf, s); a!=NULL; a=strtok(NULL,s)){
        (l->words)[i++]=a;
    }
    (l->words)[i]=(char*)NULL;/*indicar que no hay más palabras*/
    return NULL;
}

int main(void){
    Line_s line;
    int err, wstatus, fd[2], file;
    pthread_t h;
    pid_t pid, pid_log;
    char buf, message[BUF_SIZE];

    if (pipe(fd)==-1){
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_log=fork(); 
    if(pid_log<0){
        perror("fork");
        exit(EXIT_FAILURE);
    }else if(pid_log==0){
        close(fd[1]);/*cerrar el descriptor de salida en el hijo*/

        /*abrir el fichero de */
        if ((file = open(LOG_FILE, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) == -1){
            perror("open");
            exit(EXIT_FAILURE);
        }
        
        /*read(fd[0],readbuffer,sizeof(readbuffer)); yo tenia eso*/
        while(read(fd[0], &buf, 1)>0){
            write(file, &buf, 1);
        } 
        /*marcar fin de escritura?*/
        close(file);
        close(fd[0]);/*cerrar descr. de entrada*/
        exit(EXIT_SUCCESS);
    }

    close(fd[0]);/*cerrar el descriptor de entrada en el padre*/

    while(fgets(line.buf, BUF_SIZE, stdin)!=NULL){
        /*mandar por la tubería el comando*/
        dprintf(fd[1], "%s", line.buf);

        /*eliminar el \n del final*/
        line.buf[strlen(line.buf)-1]='\0';

        /*crear hilo*/
        if((err = pthread_create(&h, NULL, proc_line, &line)) != 0){
            fprintf(stderr, "pthread_create: %s\n", strerror(err));
            exit (EXIT_FAILURE);
        }
        /*esperar que termine*/
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
        /*wait:storing the status of the termination of the child*/
        if(waitpid(pid,&wstatus,0)==-1){
            perror("wait");
            exit(EXIT_FAILURE);
        }
        if(WIFEXITED(wstatus)){
            sprintf(message, "Exited with value: %d\n", WEXITSTATUS(wstatus));
            fprintf(stdout, "%s", message);
        }else if(WIFSIGNALED(wstatus)){
            sprintf(message, "Terminated by signal: %s\n", strsignal(WTERMSIG(wstatus)));
            fprintf(stderr, "%s", message);
        }
        dprintf(fd[1], "%s", message);
    }
    close(fd[1]);
    
    /*Esperar al proceso que escribe en log.txt*/
    if(waitpid(pid_log,NULL,0)==-1){
        perror("wait");
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}