# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <sys/types.h>
# include <sys/wait.h>

int main(void){
    pid_t pid;
    FILE* f=NULL;

    if((f=fopen("salida.txt", "w"))==NULL){
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    fprintf(f, "Yo soy tu padre\n");fflush(f);

    pid=fork();

    if(pid<0){
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (pid==0){
        fprintf(f, "Noooooo\n");
        exit(EXIT_SUCCESS);
    }

    wait(NULL);
    exit(EXIT_SUCCESS);
}