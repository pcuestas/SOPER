EXE=proc_shell
CC=gcc
OBJ=$(EXE).o
LIB=-lpthread
CFLAGS= -pedantic -Wall -g

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $(OBJ) $(LIB) -o $(EXE) 
	rm *.o

run: $(EXE)
	@echo "------------"
	./$(EXE)

%.o: %.c 
	$(CC) $(CFLAGS) $(LIB) -c $< -o $@

clean:
	$(RM) ./$(EXE) *.o