#include <stdio.h>

#include <stdlib.h>
#include <pthread.h>

void * hilo(void *arg){
    printf("Hola soy el hilo\n");
}


int main(){
    pthread_t h1;

    pthread_create(&h1,NULL,&hilo,NULL);
    sleep(1);
    pthread_detach(h1);

    sleep(5);

    pthread_cancel(h1);

    printf("He matado");
    return 0;
}