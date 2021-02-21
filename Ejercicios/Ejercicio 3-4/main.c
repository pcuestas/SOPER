#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv){
    long s;
    clock_t t0=clock(), t1;

    while((t1-t0)/CLOCKS_PER_SEC<10){
        t1=clock();
    }

    printf("change\n");
    fflush(stdout);

    sleep(10);

    return 0;
}