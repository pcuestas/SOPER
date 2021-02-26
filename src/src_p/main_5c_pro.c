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
    char *hola="Hola";
    char *mundo="Mundo";
    int error;
    pthread_attr_t attr_detached;/*atributo usado para llamar a pthread_create*/

    error = pthread_attr_init(&attr_detached);/*inicializamos el atributo*/
    if(error!=0){
        fprintf(stderr, "pthread_attr_getdetachstate: %s\n", strerror(error));
        exit (EXIT_FAILURE);
    }
    error = pthread_attr_setdetachstate(&attr_detached, PTHREAD_CREATE_DETACHED);/*para tener el atributo con el que se inicializa el hilo ya desligado*/
    if(error!=0){
        fprintf(stderr, "pthread_attr_getdetachstate: %s\n", strerror(error));
        exit (EXIT_FAILURE);
    }

    error = pthread_create(&h1, &attr_detached, slow_printf, hola);
    if(error != 0){
        fprintf(stderr, "pthread_create: %s\n", strerror(error));
        exit (EXIT_FAILURE);
    }
    error = pthread_create(&h2, &attr_detached, slow_printf, mundo);
    if(error != 0){
        fprintf(stderr, "pthread_create: %s\n", strerror(error));
        exit (EXIT_FAILURE);
    }

    sleep(max(strlen(hola), strlen(mundo))); /*esto es lo que tardarán como mínimo-evitamos que el hilo principal compruebe constantemente*/
    while (_h_count!=2);/*nos aseguramos de que han terminado*/
    printf("El programa %s terminó correctamente\n", argv[0]);
    exit(EXIT_SUCCESS);
}

