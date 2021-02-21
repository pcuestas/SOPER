#include <stdio.h>
#include <stdlib.h>

int main(){
    FILE *f=NULL;

    if((f=fopen("/etc/shadow","r"))==NULL){
        perror("Error al abrir el archivo");
    };
    
    if(f!=NULL)
        fclose(f);

    return 0;
}