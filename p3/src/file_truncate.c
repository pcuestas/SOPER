#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define SHM_NAME "/shm_example"
#define MESSAGE "Test message"


int main(int argc, char *argv[]) {
    int fd;
    struct stat statbuf;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <FILE>\n", argv[0]);
    }

	/* he cambiado la flag: O_EXCL por O_TRUNC */
    fd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }
    dprintf(fd, "%s", MESSAGE);
    
	/* Get size of the file. */
	
    if (fstat(fd, &statbuf) != 0) {
        perror("fstat");
	    exit(EXIT_FAILURE);
    }

	printf("Tamaño: %ld\n", statbuf.st_size);

    /* Truncate the file to size 5. */
    if (ftruncate(fd, 5) != 0) {
        perror("ftruncate");
	    exit(EXIT_FAILURE);
    }

    if (fstat(fd, &statbuf) != 0) {
        perror("fstat");
	    exit(EXIT_FAILURE);
    }


	printf("Tamaño tras truncar: %ld\n", statbuf.st_size);


    close(fd);
    exit(EXIT_SUCCESS);
}
