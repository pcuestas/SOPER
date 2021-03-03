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

    (l->words)[i]=NULL;/*indicar que no hay más palabras*/
    return NULL;
}

int main(void){
    Line_s line;
    int err, i;
    pthread_t h;

    while(fgets(line.buf, BUF_SIZE, stdin)!=NULL){
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
        
        for(i=0;line.words[i]!=NULL;i++){
            printf("%s\n", line.words[i]);
        }
    }

    exit(EXIT_SUCCESS);
}