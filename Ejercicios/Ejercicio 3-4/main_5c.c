#include  <pthread.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <unistd.h>

#define max(a,b) ((a)>(b)?(a):(b))

int _h_count=0;

void *slow_printf(void *arg) {
    const  char *msg = arg;
    size_t i;
    
    for (i = 0; i < strlen(msg); i++) {
        printf(" %c ", msg[i]);
        fflush(stdout);
        sleep (1);
    }    
    _h_count++; /*para saber cuántos han terminado (0, 1, o 2)*/
    return  NULL;
}

int main(int argc, char **argv){
    pthread_t h1, h2;
    char *hola="Hola", *mundo="Mundo";
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
    error = pthread_detach(h1);
    if(error != 0){
        fprintf(stderr, "pthread_detach: %s\n", strerror(error));
        exit (EXIT_FAILURE);
    }
    error = pthread_detach(h2);
    if(error != 0){
        fprintf(stderr, "pthread_detach: %s\n", strerror(error));
        exit (EXIT_FAILURE);
    }

    sleep(max(strlen(hola), strlen(mundo))); /*esto es lo que tardarán como mínimo-evitamos que el hilo principal compruebe constantemente*/
    while (_h_count!=2);/*nos aseguramos de que han terminado*/
    printf("El programa %s terminó correctamente\n", argv[0]);
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
