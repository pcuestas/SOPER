EXE=main
CC=gcc
OBJ=$(EXE).o
LIB=-lpthread

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $(OBJ) $(LIB) -o $(EXE) 
	rm *.o

run: $(EXE)
	@echo "------------"
	./$(EXE)

%.o: %.c 
	$(CC) -c $< -o $@

clean:
	$(RM) ./$(EXE) *.o