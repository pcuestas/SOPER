#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char **argv){
    FILE *f = NULL;
    int x;

    if (argc != 2){
        printf("enter one argument (filename)\n");
        return EXIT_FAILURE;
    }

    f=fopen(argv[1],"r"); 
    x=errno;
    printf("El valor de errno es %i",x);
    errno=x; /* Aqu'i aseguramos que la función perror() va a imprimir el código de error de fopen ya que hemos restaurado el valor de errno asociado a fopen() */
    perror();

    if (f != NULL)
        fclose(f);

    return 0;
}