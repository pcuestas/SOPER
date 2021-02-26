all: proc_example proc_example2 proc_malloc clean proc_exec

CFLAGS=-ansi -pedantic -Wall -g

proc_example: proc_example.o
	gcc proc_example.o -o proc_example

proc_malloc: proc_malloc.o
	gcc proc_malloc.o -o proc_malloc

proc_malloc.o: proc_malloc.c
	gcc proc_malloc.c -c

proc_example.o: proc_example.c
	gcc proc_example.c -c

proc_example2: proc_example_copy.o
	gcc proc_example_copy.o -o proc_example2

proc_example2.o: proc_example_copy.c
	gcc proc_example_copy.c -c

proc_exec: proc_exec.o 
	gcc proc_exec.o -o proc_exec

proc_exec.o: proc_exec.c 
	gcc proc_exec.c -c

clean:  
	rm -f *.o