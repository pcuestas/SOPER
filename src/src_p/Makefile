EXE=file_descriptors
CC=gcc
OBJ=$(EXE).o
LIB=-lpthread
CFLAGS=-ansi -pedantic -Wall -g

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