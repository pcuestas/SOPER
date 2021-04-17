#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <mqueue.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <fcntl.h>

#include "stream.h"

int main(int argc, char *argv[]){
    struct stream_t stream_struct;
    int fd_shm;

    if (argc != 2){
        fprintf(stderr, "Uso:\t%s <input_file> <output_file>", argv[0]);
        exit(EXIT_FAILURE);
    }

    /*creación del segmento de memoria compartida*/
    if ((fd_shm = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL,
                           S_IRUSR | S_IWUSR)) == -1)
    {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    /*truncar al tamaño de la estructura*/
    if (ftruncate(fd_shm, sizeof(stream_struct)) == -1)
    {
        perror(" ftruncate ");
        shm_unlink(SHM_NAME);
        exit(EXIT_FAILURE);
    }

    return 0;
}