#include  <pthread.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <unistd.h>

int first=0;
int second=0;

void *slow_printf(void *arg) {
    const  char *msg = arg;
    size_t i;
    
    for (i = 0; i < strlen(msg); i++) {
        printf(" %c ", msg[i]);
        fflush(stdout);
        sleep (1);
    }
    
    if(first==0){first=1;}
    else{second=1;}

    return  NULL;
}

int main(int argc, char **argv){
    void *i;

    pthread_t h1, h2;
    char *hola="Hola";
    char *mundo="Mundo";
    int error;

    error = pthread_create(&h1, NULL, slow_printf, hola);
    if(error != 0){
        fprintf(stderr, "pthread_create: %s\n", strerror(error));
        exit (EXIT_FAILURE);
    }

    error = pthread_create(&h2, NULL, slow_printf, mundo);
    if(error != 0){
        fprintf(stderr, "pthread_create: %s\n", strerror(error));
        exit (EXIT_FAILURE);
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

    printf("El programa %s terminó correctamente\n", argv[0]);
    pthread_exit(EXIT_SUCCESS);
}