#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char **argv){
    FILE *f = NULL;
    int error_code;

    if (argc != 2){
        printf("enter one argument (filename)\n");
        return EXIT_FAILURE;
    }

    f = fopen(argv[1], "r");
    error_code = errno;
    printf("Código del error: %d\n", error_code);

    perror("Error con código al abrir el archivo");

    if (f != NULL)
        fclose(f);

    return 0;
}