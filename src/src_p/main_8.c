#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <stdint.h>
#include  <unistd.h>
#include  <sys/types.h>
#include  <sys/wait.h>


#define MESSAGE "Hello"

int main(void){
    //char *argv[4]= { "mi-ls" , "-la", "./" , NULL};
    pid_t pid;

    pid = fork();
    if(pid<0){
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if(pid==0){
        if(execl("/bin/ls", "ls", "./" , (char*)NULL)){
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    }
    else {
        wait(NULL);
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
