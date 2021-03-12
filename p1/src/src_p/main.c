#include  <stdio.h>
#include  <stdlib.h>
#include  <string.h>
#include  <stdint.h>
#include  <unistd.h>
#include  <sys/types.h>
#include  <sys/wait.h>


int main(){
  int i, pid;
  
  for ( i = 0; i < 3; i++){
    pid=fork();
    if(!i%2) fork();
  }
  for( i = 0; i < 5 ; ++i) wait(NULL);

  printf("terminado: %jd\n", (intmax_t)getpid());

  exit(EXIT_SUCCESS);
}