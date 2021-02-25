#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <stdint.h>
#include  <unistd.h>
#include  <sys/types.h>
#include  <sys/wait.h>


#define MESSAGE "Hello"

int main(int argc, char **argv){
    pid_t pid;
    char * sentence = calloc(sizeof(MESSAGE), 1);

    pid = fork();
    if(pid<0){perror("fork");exit(EXIT_FAILURE);}
    else if(pid==0){
        strcpy(sentence, MESSAGE);
        free(sentence);
        exit(EXIT_SUCCESS);
    }
    else {
        wait(NULL);
        printf("Padre: %s\n", sentence);
        free(sentence);
        exit(EXIT_SUCCESS);
    }
    

    exit(EXIT_SUCCESS);
}

    /*error = pthread_detach(h1);
    if(error != 0){
        fprintf(stderr, "pthread_detach: %s\n", strerror(error));
        exit (EXIT_FAILURE);
    }

    error = pthread_detach(h2);
    if(error != 0){
        fprintf(stderr, "pthread_detach: %s\n", strerror(error));
        exit (EXIT_FAILURE);
    }*/
