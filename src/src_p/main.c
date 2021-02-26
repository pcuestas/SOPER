#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <stdint.h>
#include  <unistd.h>
#include  <sys/types.h>
#include  <sys/wait.h>


#define NUM_PROC 3

int main(void) {
	int i;
	pid_t pid;

	for (i = 0; i < NUM_PROC; i++) {
		pid = fork();
		if (pid <  0) {
			perror("fork");
			exit(EXIT_FAILURE);
		}
		else if (pid == 0) {
			printf("Proceso %jd, hijo de %jd\n", (intmax_t)getpid(),(intmax_t)getppid());
			exit(EXIT_SUCCESS);
		}
		else if (pid > 0) {
			printf("Padre %d\n", i);
		}
	}
	while(wait(NULL)!=-1){}
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
