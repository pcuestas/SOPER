#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MESSAGE "Hello"

int main(void) {
	pid_t pid;
	char * sentence = calloc(sizeof(MESSAGE), 1);

	pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(EXIT_FAILURE);
	}
	else if (pid == 0) {
		if(sentence==NULL){
			printf("en hijo es null\n");
		}
		strcpy(sentence, MESSAGE);
		printf("%s hijoo \n",sentence);
		free(sentence);
		exit(EXIT_SUCCESS);
	}
	else {
		wait(NULL);
		if (sentence == NULL){	
			printf("en padre es null\n");
		}
		free(sentence);
		printf("Padre: %s !\n", sentence);
		exit(EXIT_SUCCESS);
	}
}
