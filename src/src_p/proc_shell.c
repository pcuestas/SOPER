#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUF_SIZE 1024
#define MAX_WORDS 200

typedef struct Line_s_{
    char buf[BUF_SIZE];
    char *words[MAX_WORDS];
}Line_s;

void *proc_line(void *line){
    Line_s *l=line;
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
    int err, wstatus;
    pthread_t h;
    pid_t pid;

    while(fgets(line.buf, BUF_SIZE, stdin)!=NULL){
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
            if(execvp(line.words[0],line.words)){
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        }
        else{
            /*wait:storing the status of the termination of the child*/
            if(wait(&wstatus)==-1){
                perror("wait");
                exit(EXIT_FAILURE);
            }
            if(WIFEXITED(wstatus)){
                fprintf(stdout, "Exited with value: %d\n", WEXITSTATUS(wstatus));
            }else if(WIFSIGNALED(wstatus)){
                fprintf(stderr, "Terminated by signal: %s\n", strsignal(WTERMSIG(wstatus)));
            }
        }
    }

    exit(EXIT_SUCCESS);
}