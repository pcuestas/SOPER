EXE=proc_shell
CC=gcc
OBJ=$(EXE).o
LIB=-lpthread
CFLAGS=

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $(OBJ) $(LIB) -o $(EXE) 
	rm *.o

run: $(EXE)
	@echo "------------"
	./$(EXE)

%.o: %.c 
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) ./$(EXE) *.o